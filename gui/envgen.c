/* (C) Guenter Geiger <geiger@epy.co.at> */


#include <m_pd.h>
#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

/* ------------------------ envgen~ ----------------------------- */

#define NONE    0
#define ATTACK  1
#define SUSTAIN 3
#define STATES  100

#include "envgen.h"
#include "w_envgen.h"

static t_class *envgen_class;


char dumpy[2000];

/* initialize envelope with argument vector */

#include <stdio.h>

/* 
   envgen crashes frequently when reallocating memory ....
   I really don't know why, it crashes during resizebytes,
   which means it is unable to realloc() the memory ?????
   the pointer seems to be ok, I don't know what else could
   cause the problem. for the moment we prevent from reallocating
   by setting the STATES variable to 100 */

void envgen_resize(t_envgen* x,int ns)
{
     if (ns > x->args) {
	  int newargs = ns*sizeof(t_float); 

	  x->duration = resizebytes(x->duration,x->args*sizeof(t_float),newargs);
	  x->finalvalues = resizebytes(x->finalvalues,x->args*sizeof(t_float),newargs);
	  x->args = ns;  
     }
}



void envgen_totaldur(t_envgen* x,t_float dur)
{
     int i;
     float f = dur/x->duration[x->last_state];

     if (dur < 10) {
	post("envgen: duration too small %f",dur);
        return;
     }

     for (i=1;i<=x->last_state;i++)
	  x->duration[i]*=f;
}


static void envgen_dump(t_envgen* e)
{
     t_atom argv[50];
     int argc= 0;
     t_atom* a = argv;
     int i;

     SETFLOAT(a,e->finalvalues[0]);argc++;
     for (i=1;i <= e->last_state;i++) {
	  SETFLOAT(argv+argc,e->duration[i] - e->duration[i-1]);
	  argc++;
	  SETFLOAT(argv+argc,e->finalvalues[i]);
	  argc++;
     }
     outlet_list(e->out2,&s_list,argc,(t_atom*)&argv);
     
}

void envgen_init(t_envgen *x,int argc,t_atom* argv)
{
     t_float* dur;
     t_float* val;
     t_float tdur = 0;

     if (!argc) return;

     x->duration[0] = 0;

     x->last_state = argc>>1;
     envgen_resize(x,argc>>1);

     dur = x->duration;
     val = x->finalvalues;

     if (argc) {
	  *val = atom_getfloat(argv++);
	  *dur = 0.0;
     }
     dur++;val++;argc--;
     for (;argc > 0;argc--) {
	  tdur += atom_getfloat(argv++);
#ifdef DEBUG
	  post("dur =%f",tdur);
#endif
	  *dur++ = tdur; 
	  argc--;
	  if (argc > 0)
	       *val++ = atom_getfloat(argv++); 
	  else
	       *val++ = 0; 
#ifdef DEBUG
	  post("val =%f",*(val-1));
#endif

     }

}





void envgen_list(t_envgen *x,t_symbol* s, int argc,t_atom* argv)
{
     envgen_init(x,argc,argv);
     if (glist_isvisible(x->w.glist)) {
	  envgen_drawme(x, x->w.glist, 0);
     }
}


void envgen_float(t_envgen *x, t_floatarg f)
{
     int state = 0;
     while (x->duration[state] < f && state <  x->last_state) state++;

     if (state == 0 || f >= x->duration[x->last_state]) {
	  outlet_float(x->x_obj.ob_outlet,x->finalvalues[state]);
	  return;
     }
     outlet_float(x->x_obj.ob_outlet,x->finalvalues[state-1] + 
		  (f - x->duration[state-1])*
		  (x->finalvalues[state] - x->finalvalues[state-1])/ 
		  (x->duration[state] - x->duration[state-1]));
}


void envgen_bang(t_envgen *x)
{
     t_atom   a[2];
     x->x_time = 0.0;


     SETFLOAT(a,x->finalvalues[NONE]);
     SETFLOAT(a+1,0);
     outlet_list(x->x_obj.ob_outlet,&s_list,2,(t_atom*)&a);

/*       we don't force the first value anymore, so the first value
       is actually with what we have left off at the end ...
       this reduces clicks
*/
     x->x_state = ATTACK;
     x->x_val = x->finalvalues[NONE];

     SETFLOAT(a,x->finalvalues[x->x_state]);
     SETFLOAT(a+1,x->duration[x->x_state]);

     outlet_list(x->x_obj.ob_outlet,&s_list,2,(t_atom*)&a);
     clock_delay(x->x_clock,x->duration[x->x_state]);
}


static void envgen_sustain(t_envgen *x, t_floatarg f)
{
     if (f > 0 && f < x->last_state) 
	  x->sustain_state = f;
}


static void envgen_tick(t_envgen* x)
{
     t_atom a[2];
     x->x_state++;
     if (x->x_state <= x->last_state) {
	  float del = x->duration[x->x_state] - x->duration[x->x_state-1];
	  clock_delay(x->x_clock,del);
	  SETFLOAT(a,x->finalvalues[x->x_state]);
	  SETFLOAT(a+1,del);
	  
	  outlet_list(x->x_obj.ob_outlet,&s_list,2,(t_atom*)&a);
     }
     else
	  clock_unset(x->x_clock);
}

static void envgen_freeze(t_envgen* x, t_floatarg f)
{
     x->x_freeze = f;
}

static void *envgen_new(t_symbol *s,int argc,t_atom* argv)
{
    t_envgen *x = (t_envgen *)pd_new(envgen_class);

    x->args = STATES;
    x->finalvalues = getbytes( x->args*sizeof(t_float));
    x->duration = getbytes( x->args*sizeof(t_float));
#ifdef DEBUG
    post("finalvalues %x",x->finalvalues);
#endif
    /* widget */

    x->w.glist = (t_glist*) canvas_getcurrent();
    if (argc) {
      x->w.width = atom_getfloat(argv++);
      argc--;
    }
    else
      x->w.width = 140;

    if (argc) {
      x->w.height = atom_getfloat(argv++);
      argc--;
    }
    else
      x->w.height = 200;



    x->w.grabbed = 0;
    x->resizing = 0;
    /* end widget */

    if (argc)
	 envgen_init(x,argc,argv);
    else {
	 t_atom a[5];
	 SETFLOAT(a,0);
	 SETFLOAT(a+1,50);
	 SETFLOAT(a+2,1);
	 SETFLOAT(a+3,50);
	 SETFLOAT(a+4,0);
	 envgen_init(x,5,a);
    }	 

    x->x_val = 0.0;
    x->x_state = NONE;
    x->sustain_state = SUSTAIN;
    x->x_freeze = 0;

    outlet_new(&x->x_obj, &s_float);
    x->out2 = outlet_new(&x->x_obj, &s_float);

    x->x_clock = clock_new(x, (t_method) envgen_tick);
    return (x);
}


void envgen_motion(t_envgen *x, t_floatarg dx, t_floatarg dy);
void envgen_click(t_envgen *x,
    t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl,
    t_floatarg alt);
void envgen_key(t_envgen *x, t_floatarg f);


void envgen_setup(void)
{
    envgen_class = class_new(gensym("envgen"), (t_newmethod)envgen_new, 0,
    	sizeof(t_envgen), 0,A_GIMME,0);

    class_addcreator((t_newmethod)envgen_new,gensym("envgen~"),A_GIMME,0);
    class_addfloat(envgen_class, envgen_float);

    class_addbang(envgen_class,envgen_bang);
    class_addlist(envgen_class,envgen_list);
    class_addmethod(envgen_class,(t_method)envgen_sustain,gensym("sustain"),A_FLOAT);

    class_addmethod(envgen_class, (t_method)envgen_click, gensym("click"),
    	A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_motion, gensym("motion"),
    	A_FLOAT, A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_key, gensym("key"),
    	A_FLOAT, 0);

    class_addmethod(envgen_class,(t_method)envgen_totaldur,gensym("duration"),A_FLOAT,NULL);
    class_addmethod(envgen_class,(t_method)envgen_freeze,gensym("freeze"),A_FLOAT,NULL);


    envgen_setwidget();
    class_setwidget(envgen_class,&envgen_widgetbehavior);
    class_addmethod(envgen_class,(t_method)envgen_dump,gensym("dump"),A_NULL);
}