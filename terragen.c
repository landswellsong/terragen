#ifdef _WIN32
	#include <windows.h> 
#endif
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

unsigned dlist;
float cam_alpha=M_PI/4,cam_beta=0.0,cam_dist=1500.0;
int width; int height;

/* Amount of landscape points */
#define NPOINTS 10
/* Resolution of grid to generate */
#define GRIDRES 100
/* Size of the terrain, game units */
#define LIM 500.0
/* Maximum mountain height */
#define MOUNT 300.0
/* Maximum depth */
#define DEPTH -150.0

/* Uniformly random number */
double urand(double lower,double upper)
{
	return ((double)(random())/(1.0+RAND_MAX))*(upper-lower)+lower;
}

/* Sets the color based on the height given */
void set_height_color(double height)
{
	double r=0.0,g=0.0,b=0.0;
	
	/* Everything upper than 80% of mountain height is white, cause snow */
	if (height>MOUNT*0.8)
	{ r=1.0; g=1.0; b=1.0; }
	/* Everything below sealevel (0) is light-blue */
	else if (height<0)
	{ r=0.4235294; g=0.5058824; b=1.0; }
	/* The rest spans evenly from light green at sea level to dark red at 80% of height 
	 * with a yellow stop at 40% of mountain height */
	else if (height<MOUNT*0.4)
	{
		double gradient_pos=height/(MOUNT*0.4);
		r=0.01176471+gradient_pos*(0.9686275-0.01176471);
		g=0.972549+gradient_pos*(0.9686275-0.972549);
		b=0.4392157;
	}
	else
	{
		double gradient_pos=(height-MOUNT*0.4)/(MOUNT*0.4);
		r=0.9686275+gradient_pos*(0.6-0.9686275);
		g=0.9686275+gradient_pos*(0.0-0.9686275);
		b=0.4392157+gradient_pos*(0.0-0.4392157);
	}
	
	glColor4f(r,g,b,1.0);
}

struct Point { double x,y,z; };

/* Note: we only calculate XZ plane distance here */
double distance(struct Point a,struct Point b)
{
	return sqrt((b.x-a.x)*(b.x-a.x)+(b.z-a.z)*(b.z-a.z));
}

/* Makes a mapping between logical grid coordinates, the exact point and the actual storage index
 * The points are mapped as follows:
 * 1 2
 *  0     
 * 4 3 
 * TODO: ints are sorta naive
 */
int gridmap(int x,int y,short pos)
{
	switch (pos)
	{
		/* Centerpoints are layed out first */
		case 0: return GRIDRES*y+x; break;
		/* Rest follows */
		case 1: return GRIDRES*GRIDRES+(GRIDRES+1)*y+x; break;
		case 2: return GRIDRES*GRIDRES+(GRIDRES+1)*y+x+1; break;
		case 3: return GRIDRES*GRIDRES+(GRIDRES+1)*(y+1)+x+1; break;
		case 4: return GRIDRES*GRIDRES+(GRIDRES+1)*(y+1)+x; break;
		default: return 0; break;
	}
}

void create_lists()
{
	dlist=glGenLists(1);
	glNewList(dlist,GL_COMPILE);
		/* The array of landscape points */
		struct Point landscape_frame[NPOINTS];
		int i,j,k;
		
		/* Populating with random values, TODO make Y not distribute uniformly */
		for (i=0;i<NPOINTS;i++)
		{
			landscape_frame[i].x=urand(-LIM,LIM);
			landscape_frame[i].y=urand(DEPTH,MOUNT);
			landscape_frame[i].z=urand(-LIM,LIM);
		}
		
		/* The gridpoints */
		struct Point points[GRIDRES*GRIDRES+(GRIDRES+1)*(GRIDRES+1)];
		
		/* Generating the grid TODO still has repetitions in calculation */
		for (i=0;i<GRIDRES;i++)
			for (j=0;j<GRIDRES;j++)
			{
				/* Point position helpers, position is relative to center point */
				double point_pos[5][2]={ {0.5,0.5},{0,0},{1,0},{1,1},{0,1} };
				
				/* Calculating the position of all the points except the center one 
				 * as a wheighted average of landscape points given distance to them */
				for (k=1;k<5;k++)
				{
					/* Detecting the true index of the point for ease */
					int p=gridmap(i,j,k);
					/* X and Z lie on the grid, it's simple */
					points[p].x=-LIM+(i+point_pos[k][0])*2.0*LIM/GRIDRES;
					points[p].z=-LIM+(j+point_pos[k][1])*2.0*LIM/GRIDRES;
					/* For Y, we collect all the heights and weight it up */
					double sum_dist=0;
					points[p].y=0; 
					int w;
					for (w=0;w<NPOINTS;w++)
					{
						/* 1/dist^k, the closer the more importart the influence is */
						double dist=pow(distance(points[p],landscape_frame[w]),-2.5);
						sum_dist+=dist;
						points[p].y+=landscape_frame[w].y*dist;
						
					}
					points[p].y/=sum_dist;
				}
				
				/* The centerpoint is the average of all the rest */
				points[gridmap(i,j,0)].x=(points[gridmap(i,j,1)].x+points[gridmap(i,j,2)].x)/2;
				points[gridmap(i,j,0)].y=(points[gridmap(i,j,1)].y+points[gridmap(i,j,2)].y+points[gridmap(i,j,3)].y+points[gridmap(i,j,4)].y)/4;
				points[gridmap(i,j,0)].z=(points[gridmap(i,j,2)].z+points[gridmap(i,j,3)].z)/2;
			}
		
		/* Now drawing the thing up! */
		for (i=0;i<GRIDRES;i++)
			for (j=0;j<GRIDRES;j++)
			{
				/* Just drawing the fan of triangles centered at centerpoint */
				glBegin(GL_TRIANGLE_FAN);
					for (k=0;k<5;k++)
					{
						int p=gridmap(i,j,k);
						set_height_color(points[p].y);
						glVertex3f(points[p].x,points[p].y,points[p].z);
					}
					set_height_color(points[gridmap(i,j,1)].y);
					glVertex3f(points[gridmap(i,j,1)].x,points[gridmap(i,j,1)].y,points[gridmap(i,j,1)].z);
				glEnd();
			}
		
		/* Drawing the sea */
		glColor4f(0.0,0.3,0.6,0.8);
		glBegin(GL_QUADS);
			glVertex3f(-LIM,0,-LIM);
			glVertex3f(LIM,0,-LIM);
			glVertex3f(LIM,0,LIM);
			glVertex3f(-LIM,0,LIM);
		glEnd();
	glEndList();
}

void draw();

void mouse(int b,int s,int x,int y)
{
}

void motion(int x,int y)
{
	static int mx=-1,my=-1;
	if (mx<0) mx=x; if (my<0) my=y;
	int dx=mx-x,dy=my-y;
	if (dx<0) cam_beta+=M_PI*abs(dx)/64;
	if (dx>0) cam_beta-=M_PI*abs(dx)/64;
	if (dy<0) cam_alpha-=M_PI*abs(dy)/64;
	if (dy>0) cam_alpha+=M_PI*abs(dy)/64;
	mx=x;my=y;
	draw();
}

void camera()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(40.0,(float)width/height,10.0,10000.0);
	gluLookAt(cam_dist*cos(cam_alpha)*cos(cam_beta),cam_dist*sin(cam_alpha),cam_dist*cos(cam_alpha)*sin(cam_beta),0.0,0.0,0.0,0.0,cos(cam_alpha)>0?1.0:-1.0,0.0);
	glMatrixMode(GL_MODELVIEW);
}

void draw()
{
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);	
	camera();
	glCallList(dlist);
	glutSwapBuffers();
}

void keyb(int key,int mx,int my)
{
    switch (key)
    {
	case GLUT_KEY_LEFT: cam_beta+=M_PI/36; break;
	case GLUT_KEY_RIGHT: cam_beta-=M_PI/36; break;
	case GLUT_KEY_UP: cam_alpha+=M_PI/36; break;
	case GLUT_KEY_DOWN: cam_alpha-=M_PI/36; break;
	case GLUT_KEY_F1: exit(0); break;
    }
    draw();
}

void size(int w,int h)
{
    width=w; height=h;
    glViewport(0,0,w,h);
    camera();
}

int main(int argc,char *argv[])
{
#ifdef _WIN32
	randomize();
#else
	srand(time(NULL));
#endif
	glutInit(&argc,argv);
	glutInitWindowSize(700,700);
	glutInitDisplayMode(GLUT_RGB|GLUT_DOUBLE|GLUT_DEPTH);
	glutCreateWindow("Simple terrain generator");
	glClearColor(0.0,0.0,0.0,0.0);
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	create_lists();
	glutReshapeFunc(size);
	glutDisplayFunc(draw);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutSpecialFunc(keyb);
	glutMainLoop();
	return 0;
}
