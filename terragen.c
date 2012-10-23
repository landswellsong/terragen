#ifdef _WIN32
	#include <windows.h> 
#endif
#include <GL/glut.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

unsigned dlist=0;
struct camera
{
	double alpha;
	double beta;
	double dist;
} Camera = { M_PI/4,0.0,1500.0 };
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
struct TerrainVertex
{
	struct Point point,normal;
	short point_set,normal_set;
} points[GRIDRES*GRIDRES+(GRIDRES+1)*(GRIDRES+1)]; /* Global variables suck, never forget it! */

/* Note: we only calculate XZ plane distance here */
double distance(struct Point a,struct Point b)
{
	return sqrt((b.x-a.x)*(b.x-a.x)+(b.z-a.z)*(b.z-a.z));
}

/* Those are suboptimal and display some poor programming practice but we're after a demo anyway */
struct Point vec_plus(struct Point a,struct Point b)
{
	struct Point ret={ b.x+a.x, b.y+a.y, b.z+a.z };
	return ret;
}
struct Point vec_minus(struct Point a,struct Point b)
{
	struct Point ret={ b.x-a.x, b.y-a.y, b.z-a.z };
	return ret;
}
double vec_length(struct Point vec)
{
	return sqrt(vec.x*vec.x+vec.y*vec.y+vec.z*vec.z);
}
struct Point vec_cross(struct Point v,struct Point u)
{
	struct Point ret={ v.y*u.z-v.z*u.y, v.z*u.x-v.x*u.z, v.x*u.y-v.y*u.x };
	return ret;
}
struct Point vec_normalize(struct Point vec)
{
	double l=vec_length(vec);
	struct Point ret={ vec.x/l,vec.y/l,vec.z/l };
	return ret;
}
struct Point vec_mult(struct Point vec,double m)
{
	struct Point ret={ vec.x*m,vec.y*m,vec.z*m };
	return ret;
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

void generate_landscape(short algorithm)
{
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
	
	memset(&points,0,sizeof(points));
	
	/* Generating the grid */
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
				/* Not calculating redundant steps */
				if (!points[p].point_set)
				{
					struct Point *point=&points[p].point;
					/* X and Z lie on the grid, it's simple */
					point->x=-LIM+(i+point_pos[k][0])*2.0*LIM/GRIDRES;
					point->z=-LIM+(j+point_pos[k][1])*2.0*LIM/GRIDRES;
					/* For Y, we collect all the heights and weight it up */
					double sum_dist=0;
					point->y=0; 
					int w;
					for (w=0;w<NPOINTS;w++)
					{
						/* 1/dist^k, the closer the more importart the influence is */
						double dist=pow(distance(*point,landscape_frame[w]),-2.5);
						sum_dist+=dist;
						point->y+=landscape_frame[w].y*dist;
						
					}
					point->y/=sum_dist;
					
					/* Marking as processed */
					points[p].point_set=1;
				}
			}
			
			/* The centerpoint is the average of all the rest */
			if (!points[gridmap(i,j,0)].point_set)
			{
				struct Point *pts[]={ &points[gridmap(i,j,0)].point,
										&points[gridmap(i,j,1)].point,
										&points[gridmap(i,j,2)].point,
										&points[gridmap(i,j,3)].point,
										&points[gridmap(i,j,4)].point};
				
				pts[0]->x=(pts[1]->x+pts[2]->x)/2;
				pts[0]->y=(pts[1]->y+pts[2]->y+pts[3]->y+pts[4]->y)/4;
				pts[0]->z=(pts[2]->z+pts[3]->z)/2;
			}
		}
		
	/* Now when all the points are ready, we can do normals */
	for (i=0;i<GRIDRES;i++)
		for (j=0;j<GRIDRES;j++)
		{
			/* Calculating the normal of all the points */
			for (k=0;k<5;k++)
			{
				/* Detecting the true index of the point for ease */
				int p=gridmap(i,j,k);
				/* Not calculating redundant steps */
				if (!points[p].normal_set)
				{
					struct Point *normal=&points[p].normal;
					
					/* TODO prettyfy this, I'm sure there's a way. I know this is horrific */
					if (k==0)
					{
						*normal=vec_plus(*normal,vec_cross(vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,2)].point),
												vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,1)].point)));
						*normal=vec_plus(*normal,vec_cross(vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,3)].point),
												vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,2)].point)));
						*normal=vec_plus(*normal,vec_cross(vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,4)].point),
												vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,3)].point)));
						*normal=vec_plus(*normal,vec_cross(vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,1)].point),
												vec_minus(points[gridmap(i,j,0)].point,points[gridmap(i,j,4)].point)));
					} else continue;
						
					*normal=vec_normalize(*normal);
					points[p].normal_set=1;
				}
			}
		}
}

void create_lists(short wireframe,short normals)
{
	if (dlist==0)
		glDeleteLists(dlist,1);
	dlist=glGenLists(1);
	/* Drawing the thing up! */
	glNewList(dlist,GL_COMPILE);
		int i,j,k;
		for (i=0;i<GRIDRES;i++)
			for (j=0;j<GRIDRES;j++)
			{
				/* Just drawing the fan of triangles centered at centerpoint */
				glBegin(wireframe? GL_LINE_LOOP : GL_TRIANGLE_FAN);
					/* If it's wireframe, we don't care about the centerpoint */
					for (k=wireframe? 1: 0;k<5;k++)
					{
						int p=gridmap(i,j,k);
						set_height_color(points[p].point.y);
						glVertex3f(points[p].point.x,points[p].point.y,points[p].point.z);
					}
					set_height_color(points[gridmap(i,j,1)].point.y);
					glVertex3f(points[gridmap(i,j,1)].point.x,points[gridmap(i,j,1)].point.y,points[gridmap(i,j,1)].point.z);
				glEnd();
				/* Drawing normal vectors if necessary */
				if (normals)
				{
					glColor4f(1.0,1.0,1.0,1.0);
					glBegin(GL_LINES);
						for (k=wireframe? 1: 0;k<5;k++)
						{
							int p=gridmap(i,j,k);
							if (points[p].normal_set)
							{
								struct Point pp=points[p].point;
								struct Point vp=vec_plus(points[p].point,vec_mult(points[p].normal,5));
								glVertex3f(pp.x,pp.y,pp.z);
								glVertex3f(vp.x,vp.y,vp.z);
							}
						}
						
					glEnd();
				}
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
	if (b==3 || b==4)
	{
		Camera.dist*= (b==4)? 1.1 : 0.9;
		draw();
	}
}

void motion(int x,int y)
{
	static int mx=-1,my=-1;
	if (mx<0) mx=x; if (my<0) my=y;
	int dx=mx-x,dy=my-y;
	if (dx<0) Camera.beta+=M_PI*abs(dx)/64;
	if (dx>0) Camera.beta-=M_PI*abs(dx)/64;
	if (dy<0) Camera.alpha-=M_PI*abs(dy)/64;
	if (dy>0) Camera.alpha+=M_PI*abs(dy)/64;
	mx=x;my=y;
	draw();
}

void camera()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(40.0,(float)width/height,10.0,10000.0);
	gluLookAt(Camera.dist*cos(Camera.alpha)*cos(Camera.beta),Camera.dist*sin(Camera.alpha),Camera.dist*cos(Camera.alpha)*sin(Camera.beta),0.0,0.0,0.0,0.0,cos(Camera.alpha)>0?1.0:-1.0,0.0);
	glMatrixMode(GL_MODELVIEW);
}

void draw()
{
	glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);	
	camera();
	glCallList(dlist);
	glutSwapBuffers();
}

void keybs(int key,int mx,int my)
{
    switch (key)
    {
	case GLUT_KEY_LEFT: Camera.beta+=M_PI/36; break;
	case GLUT_KEY_RIGHT: Camera.beta-=M_PI/36; break;
	case GLUT_KEY_UP: Camera.alpha+=M_PI/36; break;
	case GLUT_KEY_DOWN: Camera.alpha-=M_PI/36; break;
	case GLUT_KEY_F1: exit(0); break;
    }
    draw();
}

void keyb(unsigned char key,int mx,int my)
{
	static short wireframe=0,normals=0;
    switch (key)
    {
	case 'w': case 'W': wireframe=!wireframe; create_lists(wireframe,normals); break;
	case 'n': case 'N': normals=!normals; create_lists(wireframe,normals); break;
	case '-': Camera.dist*= 1.1; break;
	case '+': Camera.dist*= 0.9; break;
	case 'r': case 'R': generate_landscape(0); create_lists(wireframe,normals); break;
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
	
	generate_landscape(0);
	create_lists(0,0);
	
	glutReshapeFunc(size);
	glutDisplayFunc(draw);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutSpecialFunc(keybs);
	glutKeyboardFunc(keyb);
	glutMainLoop();
	return 0;
}
