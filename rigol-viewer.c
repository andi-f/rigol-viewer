// rigol-viewer.c - a gtk-application for readout some data from Rigol DS1000 series scopes 
// The function rigol_read and rigol_write was written  by Mark Whitis, 2012 by Jiri Pittner <jiri@pittnerovi.com>
// See http://www.pittnerovi.com/jiri/hobby/electronics/rigol/rigol.c

/*
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 * 
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>      
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/usb/tmc.h>
#include <gtk/gtk.h>
#include <cairo/cairo.h>

#define NODEBUG 

#define MAX_DEVICE_SEARCH 10

#define PROGRAM "Rigol DSO viewer"
#define VERSION  "v 1.3 GTK"
#define COPYRIGHT "Andreas Fischer 11/2016"
#define COMMENT "Viewer for Rigol DS1000 serie"
#define URI	"https://github.com/andi-f/rigol-viewer"

#define HEIGHT 600
#define WIDTH	800

typedef struct {
	unsigned char *buffer;			//buffer for data
	long *length;								//length of data
	double *time_scale;	
	double *time_offset;
	double *voltage_scale;
	double *voltage_offset;

	int	*device;								// /dev/usbtmcx
	long *mode;									// 0 time 1 frequency 2 LA
	
	GtkWidget *darea;						// gtk drawing area
	GtkWidget *statusbar;				// gtk statusbar
	GtkWidget *filesel;					// gtk statusbar
} rigol_data;

int rigol_read(int handle, unsigned char *buf, size_t size);
int rigol_write(int handle, unsigned char *string);

void get_channel1(GtkWidget *widget, rigol_data *data);
void get_channel2(GtkWidget *widget, rigol_data *data);
void get_channel_data(rigol_data *data,char *channel);

void get_fft(GtkWidget *widget, rigol_data *data);

void get_la(GtkWidget *widget, rigol_data *data);

void find_DS1000(rigol_data *data);

void filesel_open(GtkWidget *widget, rigol_data *data);

static gboolean drawing_screen(GtkWidget *widget, GdkEventExpose *event,  rigol_data *data);
void create_png(GtkWidget *widget, rigol_data *data);
void plot(cairo_t *cr, rigol_data *data, gint	width, gint height);
void plot_la(cairo_t *cr, rigol_data *data, gint	width, gint height);

void about(GtkWidget *widget, gpointer data);	

int main(int argc, char *argv[]) {
	
	rigol_data data;
	
  GtkWidget *window;
	GtkWidget *vbox1;
  GtkWidget *darea;
	GtkWidget *menubar1;	
	
	GtkWidget *menu_quit;
	GtkWidget *menuitem_quit;
	
	GtkWidget *menu_ch;
	GtkWidget *menuitem_ch;

	GtkWidget *menu_ch1_set;
	GtkWidget *menuitem_ch1_set;

	GtkWidget *menu_ch2_set;
	GtkWidget *menuitem_ch2_set;

	GtkWidget *menu_time_set;
	GtkWidget *menuitem_time_set;

	GtkWidget *menu_png;
	GtkWidget *menuitem_png;

	GtkWidget *menu_about;
	GtkWidget *menuitem_about;

	GtkWidget *statusbar;

	data.buffer = (unsigned char *) malloc(1049600 * sizeof(unsigned char));
	data.length = (long *) malloc(sizeof(long));
	data.time_scale = (double *) malloc(sizeof(double));
	data.time_offset = (double *) malloc(sizeof(double));
	data.voltage_scale = (double *) malloc(sizeof(double));
	data.voltage_offset = (double *) malloc(sizeof(double));
	data.device = (int *) malloc(sizeof(int));
	data.mode = (long *) malloc(sizeof(long));
	
	//Searching for Rigol DS1000 series scopes 
	find_DS1000(&data);
	
  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(window, "destroy",
      G_CALLBACK(gtk_main_quit), NULL);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox1);

	menubar1 = gtk_menu_bar_new();
	gtk_box_pack_start(GTK_BOX(vbox1), menubar1, FALSE, FALSE, 2);

  darea = gtk_drawing_area_new();
	gtk_box_pack_start (GTK_BOX(vbox1), darea, TRUE, TRUE, 0);    	

	gtk_widget_set_app_paintable(darea, TRUE);
	g_signal_connect(darea, "expose-event", G_CALLBACK(drawing_screen),(gpointer*) &data);
	gtk_widget_set_app_paintable(darea, TRUE);

  statusbar = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(vbox1), statusbar, FALSE, TRUE, 1);

  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), WIDTH, HEIGHT);

  gtk_window_set_title(GTK_WINDOW(window), PROGRAM);

	//Quit
	menu_quit = gtk_menu_new();

	menuitem_quit = gtk_menu_item_new_with_label("Quit");
	gtk_menu_append(GTK_MENU(menu_quit), menuitem_quit);
	
	gtk_signal_connect(GTK_OBJECT(menuitem_quit), "activate",
                     GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	menuitem_quit = gtk_menu_item_new_with_label("Quit");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem_quit),
			    menu_quit);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar1), menuitem_quit);

	//Select source
	menu_ch = gtk_menu_new();

	menuitem_ch = gtk_menu_item_new_with_label("Channel 1");
	gtk_signal_connect(GTK_OBJECT(menuitem_ch), "activate",
                     G_CALLBACK(get_channel1),(gpointer*) &data);
	gtk_menu_append(GTK_MENU(menu_ch), menuitem_ch);


	menuitem_ch = gtk_menu_item_new_with_label("Channel 2");
	gtk_signal_connect(GTK_OBJECT(menuitem_ch), "activate",
                     G_CALLBACK(get_channel2),(gpointer*) &data);
	gtk_menu_append(GTK_MENU(menu_ch), menuitem_ch);


	menuitem_ch = gtk_menu_item_new_with_label("FFT");
	gtk_signal_connect(GTK_OBJECT(menuitem_ch), "activate",
                     G_CALLBACK(get_fft),(gpointer*) &data);
	gtk_menu_append(GTK_MENU(menu_ch), menuitem_ch);


	menuitem_ch = gtk_menu_item_new_with_label("LA");
	gtk_signal_connect(GTK_OBJECT(menuitem_ch), "activate",
                     G_CALLBACK(get_la),(gpointer*) &data);
	gtk_menu_append(GTK_MENU(menu_ch), menuitem_ch);


	menuitem_ch = gtk_menu_item_new_with_label("Select source");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem_ch),
			    menu_ch);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar1), menuitem_ch);

	//Channel
	menu_ch1_set = gtk_menu_new();

	menuitem_ch1_set = gtk_menu_item_new_with_label("100V/div");
	gtk_signal_connect(GTK_OBJECT(menuitem_ch1_set), "activate",
                     G_CALLBACK(menu_ch1_set),10);
	gtk_menu_append(GTK_MENU(menu_ch1_set), menuitem_ch1_set);


	menuitem_ch1_set = gtk_menu_item_new_with_label("Channel 1");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem_ch1_set),
			    menu_ch1_set);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar1), menuitem_ch1_set);


	//PNG
	menu_png = gtk_menu_new();

	menuitem_png = gtk_menu_item_new_with_label("Create PNG");
	gtk_signal_connect(GTK_OBJECT(menuitem_png), "activate",
                     G_CALLBACK(create_png), (gpointer*) &data);
	gtk_menu_append(GTK_MENU(menu_png), menuitem_png);

	menuitem_png = gtk_menu_item_new_with_label("Create PNG");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem_png),
			    menu_png);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar1), menuitem_png);

	//About
	menu_about = gtk_menu_new();
	menuitem_about = gtk_menu_item_new_with_label("Info");
	gtk_signal_connect(GTK_OBJECT(menuitem_about), "activate",
                     GTK_SIGNAL_FUNC(about), NULL);
	gtk_menu_append(GTK_MENU(menu_about), menuitem_about);

	menuitem_about = gtk_menu_item_new_with_label("About");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem_about),
			    menu_about);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar1), menuitem_about);

  
	data.darea = darea;
  data.statusbar = statusbar;
  
  gtk_widget_show_all(window);

  gtk_main();
  
  free(data.buffer);
  free(data.length);
	free(data.time_scale);
	free(data.time_offset);
	free(data.voltage_scale);
	free(data.voltage_offset);
	free(data.device);
	free(data.mode);

	return 0;
}

// Callback for about
void about(GtkWidget *widget, gpointer data) {
  GtkWidget *dialog = gtk_about_dialog_new();
  gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), PROGRAM);
  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), VERSION); 
  gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), COPYRIGHT);
  gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), COMMENT);
  gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), URI);
  gtk_dialog_run(GTK_DIALOG (dialog));
  gtk_widget_destroy(dialog);

 }

// Get channel1	
void get_channel1(GtkWidget *widget, rigol_data *data) {
	char device[20]="";
	
	get_channel_data(data,"CHAN1");
	sprintf(device,"/dev/usbtmc%d",*data->device);
	gtk_statusbar_push(GTK_STATUSBAR(data->statusbar),gtk_statusbar_get_context_id(GTK_STATUSBAR(data->statusbar), "Stopped"),"Stopped");
	
	gtk_widget_queue_draw_area(data->darea, 0, 0, WIDTH, HEIGHT);
	
	return;	
}

// Get channel2	
void get_channel2(GtkWidget *widget, rigol_data *data) {
	char device[20]="";
	
	get_channel_data(data,"CHAN2");
	sprintf(device,"/dev/usbtmc%d",*data->device);
	gtk_statusbar_push(GTK_STATUSBAR(data->statusbar),gtk_statusbar_get_context_id(GTK_STATUSBAR(data->statusbar), device),device);
	
	gtk_widget_queue_draw_area(data->darea, 0, 0, WIDTH, HEIGHT);
	
	return;
}

// Reading data for channel 1+2
void get_channel_data(rigol_data *data,char *channel)	{
	const int max_response_length=1024*1024+1024;
	char device[20]="";
	int handle;
	long rc;
	int i;
	unsigned char buf[max_response_length];
	double dummy_double;

	sprintf(device,"/dev/usbtmc%d",*data->device);
	#ifdef DEBUG	
	g_print("%s\n",device);
	#endif			
	
	handle=open(device, O_RDWR);

	if(handle<0) {
		perror("error opening device");
		return;
	}

	rigol_write(handle, ":STOP");

	rigol_write(handle, ":WAV:POIN:MODE NOR");
	
	sprintf(buf,":WAV:DATA? %s",channel);
	#ifdef DEBUG	
	g_print("%s\n",buf);
	#endif			

	rigol_write(handle, buf);
	rc=rigol_read(handle, buf,max_response_length);
	#ifdef DEBUG		
	for(i=0;i<rc;i++) {//10
		g_print(" %02X ",buf[i]);
		if( (i%24)==31 )
			g_print("\n");
	}		
	g_print("\n");
	g_print("%li\n", rc);				
	#endif			
	memcpy(data->buffer, buf,rc);	
	memcpy(data->length, &rc, sizeof(rc));		

	sprintf(buf,":%s:SCAL?",channel);
	#ifdef DEBUG	
	g_print("%s\n",buf);
	#endif			
	rigol_write(handle,buf);
	rc=rigol_read(handle, buf,max_response_length);	
	#ifdef DEBUG	
	g_print("Voltage: %s V/DIV\n", buf);	
	#endif		
	dummy_double = atof(buf);
	memcpy(data->voltage_scale, &dummy_double, sizeof(dummy_double));		

	sprintf(buf,":%s:OFFS?",channel);
	#ifdef DEBUG	
	g_print("%s\n",buf);
	#endif			
	rigol_write(handle,buf);
	rc=rigol_read(handle, buf,max_response_length);	
	#ifdef DEBUG
	g_print("Offset %s V\n", buf);
	#endif	
	dummy_double = atof(buf);
	memcpy(data->voltage_offset, &dummy_double, sizeof(dummy_double));		
		
	rigol_write(handle,":TIM:SCAL?");
	rc=rigol_read(handle, buf,max_response_length);	
	#ifdef DEBUG	
	g_print("Time: %s s/DIV\n", buf);
	#endif		
	dummy_double = atof(buf);
	memcpy(data->time_scale, &dummy_double, sizeof(dummy_double));		

	rigol_write(handle,":TIM:OFFS?");
	rc=rigol_read(handle, buf,max_response_length);	
	#ifdef DEBUG	
	g_print("Time offset %s s\n", buf);
	#endif		
	dummy_double = atof(buf);
	memcpy(data->time_offset, &dummy_double, sizeof(dummy_double));		

	rigol_write(handle, ":RUN");
		
	rigol_write(handle,":KEY:LOCK DISABLE");
	
	close(handle);	

	rc=0;	//time-scale
	memcpy(data->mode, &rc, sizeof(rc));		
	
	return;		
}

// Reading FFT
void get_fft(GtkWidget *widget, rigol_data *data) {
	const int max_response_length=1024*1024+1024;
	char device[20]="";
	int handle;
	long rc;
	int i;
	unsigned char buf[max_response_length];
	
	sprintf(device,"/dev/usbtmc%d",*data->device);
	handle=open(device, O_RDWR);

	if(handle<0) {
		perror("error opening device");
		return;
	}

	rigol_write(handle, ":MAT:DISP ON");
	
	rigol_write(handle, ":MAT:OPER FFT");
	
	rigol_write(handle, ":FFT:DISP ON");
		
	rigol_write(handle, ":STOP");

	rigol_write(handle, ":WAV:POIN:MODE NOR");		//can be NOR RAW MAX
	
	rigol_write(handle, ":WAV:DATA? FFT");
	rc=rigol_read(handle, buf,max_response_length);
#ifdef DEBUG		
	for(i=0;i<rc;i++) {//10
		g_print(" %02X ",buf[i]);
		if( (i%24)==31 )
			g_print("\n");
	}		
	g_print("\n");
	g_print("%li\n", rc);				
#endif			
	memcpy(data->buffer, buf,rc);
	memcpy(data->length, &rc, sizeof(rc));		

	rigol_write(handle, ":RUN");
		
	rigol_write(handle,":KEY:LOCK DISABLE");
	
	close(handle);	

	rc=1;	//FFT
 	memcpy(data->mode, &rc, sizeof(rc));		
	
	gtk_statusbar_push(GTK_STATUSBAR(data->statusbar),gtk_statusbar_get_context_id(GTK_STATUSBAR(data->statusbar), device),device);	
		
	gtk_widget_queue_draw_area(data->darea, 0, 0, WIDTH, HEIGHT);
	
  return;	
}

// Reading Logicanalyser
void get_la(GtkWidget *widget, rigol_data *data) {
	const int max_response_length=1024*1024+1024;
	char device[20]="";
	int handle;
	long rc;
	int i;
	unsigned char buf[max_response_length];
	double dummy_double;
	
	sprintf(device,"/dev/usbtmc%d",*data->device);
	handle=open(device, O_RDWR);

	if(handle<0) {
		perror("error opening device");
		return;
	}

	//Display all digital inputs
	rigol_write(handle, ":LA:DISP ON");
	
	rigol_write(handle, ":LA:GROU1 ON");
	rigol_write(handle, ":LA:GROU1:SIZ S");		
	
	rigol_write(handle, ":LA:GROU2 ON");
	rigol_write(handle, ":LA:GROU2:SIZ S");		

	rigol_write(handle, ":LA:POS:RES");

	rigol_write(handle, ":STOP");

	rigol_write(handle, ":WAV:POIN:MODE NOR");		//can be NOR RAW MAX
	
	rigol_write(handle, ":WAV:DATA? DIG");
	rc=rigol_read(handle, buf,max_response_length);
#ifdef DEBUG		
	for(i=0;i<rc;i++) {//10
		g_print(" %02X ",buf[i]);
		if( (i%24)==31 )
			g_print("\n");
	}		
	g_print("\n");
	g_print("%li\n", rc);				
#endif			
	memcpy(data->buffer, buf,rc);
	memcpy(data->length, &rc, sizeof(rc));		

	rigol_write(handle,":TIM:SCAL?");
	rc=rigol_read(handle, buf,max_response_length);	
	#ifdef DEBUG	
	g_print("Time: %s s/DIV\n", buf);
	#endif		
	dummy_double = atof(buf);
	memcpy(data->time_scale, &dummy_double, sizeof(dummy_double));		

	rigol_write(handle,":TIM:OFFS?");
	rc=rigol_read(handle, buf,max_response_length);	
	#ifdef DEBUG	
	g_print("Time offset %s s\n", buf);
	#endif		
	dummy_double = atof(buf);
	memcpy(data->time_offset, &dummy_double, sizeof(dummy_double));		

	rigol_write(handle, ":RUN");
		
	rigol_write(handle,":KEY:LOCK DISABLE");
	
	close(handle);	

	rc=2;	//LA
 	memcpy(data->mode, &rc, sizeof(rc));		
	
	gtk_statusbar_push(GTK_STATUSBAR(data->statusbar),gtk_statusbar_get_context_id(GTK_STATUSBAR(data->statusbar), device),device);	
		
	gtk_widget_queue_draw_area(data->darea, 0, 0, WIDTH, HEIGHT);
	
  return;	
}

// Reading device
int rigol_write(int handle, unsigned char *string) {
	int rc;
	unsigned char buf[256];
	
	strncpy(buf,string,sizeof(buf));
	buf[sizeof(buf)-1]=0;

	strncat(buf,"\n",sizeof(buf));
	buf[sizeof(buf)-1]=0;

	rc=write(handle, buf, strlen(buf));
	if(rc<0) 
		perror("write error");
	return(rc);
}

//Writing device
int rigol_read(int handle, unsigned char *buf, size_t size) {
	int rc;
	if(!size)
		return(-1);
		
	buf[0]=0;
	rc=read(handle, buf, size);
	if((rc>0) && (rc<size))
		buf[rc]=0;
	if(rc<0) {
		if(errno==ETIMEDOUT)
			g_print("No response\n");
		else {
			perror("read error");
		}
	}
	// g_print("rc=%d\n",rc);
	return(rc);
}
 
// Redraw Screen
static gboolean drawing_screen(GtkWidget *widget, GdkEventExpose *event, rigol_data *data) {
	gint 			width, 
						height;						// Window width & height
	
	cairo_t *cr;
	cr = gdk_cairo_create(widget->window);
  
	gdk_drawable_get_size(widget->window, &width, &height);
	
	if((*data->mode == 0) ||(*data->mode == 1))
		plot(cr, data,width,height);
	else
	if(*data->mode == 2)
		plot_la(cr, data,width,height);
	
	cairo_destroy(cr);

  return TRUE;
}

// Create image
void create_png(GtkWidget *widget, rigol_data *data) {
	gint 			width = 800, 
						height = 600;			// Window width & 
	
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cr = cairo_create(surface);

	if(*data->mode == 0) {
		plot(cr, data,width,height);
		cairo_surface_write_to_png(surface, "~/image-channel.png");
	}
	else
	if(*data->mode == 1)	{
		plot(cr, data,width,height);
		cairo_surface_write_to_png(surface, "~/image-fft.png");
	}
	else	
	if(*data->mode == 2)	{
		plot_la(cr, data,width,height);
		cairo_surface_write_to_png(surface, "~/image-LA.png");
	}
	
	cairo_surface_destroy(surface);
	cairo_destroy(cr);

  return;
}

// Plot Data from Channelx and FFT
void plot(cairo_t *cr, rigol_data *data, gint	width, gint height) {
	double		A;								// actual value
	
	double		X_DIV = 10.0;			// 10 div/x
	double		Y_DIV = 8.0;			// 8 div/y

	double		MAX_Y = 225.0;		// max Y scale we know from experimentation that the scope display range is actually 25-225
	double		MIN_Y = 25.0;			// min Y scale 
	double		MAX_X = 10.0;			// max X scale
	double		MIN_X = 0.0;			// min X scale

	double 		XMAX;							// max dots x-scale
	double 		YMAX;							// max dots y-scale
	double 		XOFFSET;					// start of diagramm
	double		YOFFSET;					// start of diagramm
	double		x, y;							// actual plot postion
	double		x_alt,y_alt;			// last plot postion
	double		x_scale,y_scale;	// scale for axis
	double		x_zero,y_zero;		// point of origin
	char			string_buf[80];		// line buffer
	unsigned char *ptr;					// pointer to Data
	long			n = 0;							
	long			m = 0;							
	
	ptr = data->buffer;
	n = *data->length;

	ptr = ptr + 10;							//real data begins with offset + 10
	n = n - 10;
	
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
 
	cairo_set_font_size(cr, 10.0);
	cairo_set_line_width(cr, 1.0);

	XMAX = 0.90 * width;
	YMAX = 0.80 * height;
	
	XOFFSET = 0.5 * (width-XMAX);
	YOFFSET = 0.5 * (height-YMAX);

	x_scale = XMAX / (MAX_X-MIN_X) * MAX_X / n;
	y_scale = YMAX / (MAX_Y-MIN_Y);	

	x_zero = XOFFSET + MIN_X * x_scale;
	y_zero = YOFFSET + YMAX/2.0;
	
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	
	sprintf(string_buf,"%s %s",PROGRAM,VERSION);
	cairo_move_to (cr,10, 0.2* YOFFSET);
	cairo_show_text(cr, string_buf); 	

	cairo_stroke(cr);

	for(m = 0; m < X_DIV+1;m ++)
	{
		x =  m * XMAX / X_DIV + XOFFSET;
		if( (x >= XOFFSET) && (x <= XOFFSET+XMAX) )
		{
			cairo_move_to(cr,x,YOFFSET);
			cairo_line_to(cr,x,YMAX+YOFFSET);
		}
	}

	for(m = 0; m < Y_DIV+1;m ++)
	{
		y = m * YMAX / Y_DIV + YOFFSET;

		cairo_move_to(cr,XOFFSET,y);
		cairo_line_to (cr,XMAX+XOFFSET,y);
	}

	cairo_stroke(cr);

	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);

	A = (*ptr-125);
	
	x_alt =  x_zero;
	y_alt =  y_zero + A * y_scale;

	if(x_alt < XOFFSET)
		x_alt = XOFFSET;
	else
	if(x_alt > (XOFFSET+XMAX) )
		x_alt = XOFFSET+YMAX;

	if(y_alt < YOFFSET)
		y_alt = YOFFSET;
	else
	if(y_alt > (YOFFSET+YMAX) )
		y_alt = YOFFSET+YMAX;
		
	ptr++;

	for(m = 1; m <= n-1; m ++)	{
		A = (*ptr-125);

		x =  x_zero + m * x_scale;
		y =  y_zero + A * y_scale;

		ptr++;

		if(x < XOFFSET)
			x = XOFFSET;
		else
		if(x > (XOFFSET+XMAX) )
			x = XOFFSET+YMAX;

		if(y < YOFFSET)
			y = YOFFSET;
		else
		if(y > (YOFFSET+YMAX) )
			y = YOFFSET+YMAX;

		cairo_move_to(cr,x_alt,y_alt);
		cairo_line_to (cr,x,y);					
		
		x_alt = x;
		y_alt = y;
	}
	cairo_stroke(cr);		

	if(*data->mode == 0)	{
		if(*data->voltage_scale < 1E-3)
			sprintf(string_buf,"Voltage: %3.1f mV/DIV",*data->voltage_scale*1E3);	
		else
			sprintf(string_buf,"Voltage: %3.1f V/DIV",*data->voltage_scale);

		cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
		cairo_move_to (cr,XOFFSET, YMAX + 1.4 *YOFFSET);
		cairo_show_text(cr, string_buf); 	

		if(fabs(*data->voltage_offset) < 1E-3)
			sprintf(string_buf,"Voltage offset: %3.1f mV",*data->voltage_offset*1E3);	
		else
			sprintf(string_buf,"Voltage : %3.1f V",*data->voltage_offset);

		cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
		cairo_move_to (cr,XOFFSET, YMAX + 1.7 *YOFFSET);
		cairo_show_text(cr, string_buf); 			

		
		if(*data->time_scale < 1E-6)
			sprintf(string_buf,"Time: %3.1f ns/DIV",*data->time_scale*1E9);	
		else
		if(*data->time_scale < 1E-3)
			sprintf(string_buf,"Time: %3.1f µs/DIV",*data->time_scale*1E6);
		else
		if(*data->time_scale < 1)
			sprintf(string_buf,"Time: %3.1f ms/DIV",*data->time_scale*1E3);
		else
			sprintf(string_buf,"Time: %3.1f s/DIV",*data->time_scale);

		cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);			
		cairo_move_to (cr,XMAX/2, YMAX + 1.4 *YOFFSET);
		cairo_show_text(cr, string_buf); 	

		if(fabs(*data->time_offset) < 1E-3)
			sprintf(string_buf,"Time offset: %3.1f µs",*data->time_offset*1E6);
		else
		if(fabs(*data->time_offset) < 1)
			sprintf(string_buf,"Timet: %3.1f ms",*data->time_offset*1E3);
		else
			sprintf(string_buf,"Timet: %3.1f s",*data->time_offset);		
	
		cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
		cairo_move_to (cr,XMAX/2, YMAX + 1.7 *YOFFSET);
		cairo_show_text(cr, string_buf); 	
	}
}

// Plot Data from LA
void plot_la(cairo_t *cr, rigol_data *data, gint	width, gint height) {
	double		A;								// actual value
	
	double		X_DIV = 1.0;			// 10 div/x
	double		Y_DIV = 1.0;			// 1 div/y

	double		MAX_Y = 16.0;			// max Y scale LA data has 16 bits
	double		MIN_Y = 0.0;			// min Y scale 
	double		MAX_X = 10.0;			// max X scale
	double		MIN_X = 0.0;			// min X scale

	double 		XMAX;							// max dots x-scale
	double 		YMAX;							// max dots y-scale
	double 		XOFFSET;					// start of diagramm
	double		YOFFSET;					// start of diagramm
	double		x, y;							// actual plot postion
	double		x_alt,y_alt;			// last plot postion
	double		x_scale,y_scale;	// scale for axis
	double		x_zero,y_zero;		// point of origin
	char			string_buf[80];		// line buffer
	unsigned char *ptr;					// pointer to Data
	long			n = 0;						// number of data
	long			m = 0;						// counter
	int				o = 0;						// bit
	int				p = 0;						// 

	ptr = data->buffer;
	n = *data->length;

	ptr = ptr + 10;							//real data begins with offset + 10
	n = (n - 10)/2;							//LA data are splited in to byte, LSB/MSB	
		
	cairo_rectangle(cr, 0.0, 0.0, width, height);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_fill(cr);

	cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
 
	cairo_set_font_size(cr, 10.0);
	cairo_set_line_width(cr, 1.0);

	XMAX = 0.90 * width;
	YMAX = 0.80 * height;
	
	XOFFSET = 0.5 * (width-XMAX);
	YOFFSET = 0.5 * (height-YMAX);

	x_scale = XMAX / (MAX_X-MIN_X) * MAX_X / n;
	y_scale = YMAX / (MAX_Y-MIN_Y);	

	x_zero = XOFFSET + MIN_X * x_scale;
	y_zero = YOFFSET + YMAX;
	
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	
	sprintf(string_buf,"%s %s",PROGRAM,VERSION);
	cairo_move_to (cr,10, 0.2* YOFFSET);
	cairo_show_text(cr, string_buf); 	

	cairo_stroke(cr);

	for(m = 0; m < X_DIV+1;m ++)
	{
		x =  m * XMAX / X_DIV + XOFFSET;
		if( (x >= XOFFSET) && (x <= XOFFSET+XMAX) )
		{
			cairo_move_to(cr,x,YOFFSET);
			cairo_line_to(cr,x,YMAX+YOFFSET);
		}
	}

	for(m = 0; m < Y_DIV+1;m ++)
	{
		y = m * YMAX / Y_DIV + YOFFSET;

		cairo_move_to(cr,XOFFSET,y);
		cairo_line_to (cr,XMAX+XOFFSET,y);
	}

	cairo_stroke(cr);

	for(o = 0; o < 16;o ++)
	{
		sprintf(string_buf,"D%1i",o);
		cairo_move_to (cr,0, y_zero - o * y_scale);
		cairo_show_text(cr, string_buf); 	
	}
//	cairo_stroke(cr);

	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);

	for(o = 0;o <8; o++)
	{
		ptr = data->buffer;
		n = *data->length;

		ptr = ptr + 10;							//real data begins with offset + 10
		n = (n - 10)/2;							//LA data are splited in to byte, LSB/MSB		
		
		p = (int) pow(2,o);

		if (*ptr & p)	{
			A = 0.9 * y_scale;
		}
		else	{
			A = 0;			
		}
	
		x_alt =  x_zero;
		y_alt =  y_zero - o * y_scale - A;

		if(x_alt < XOFFSET)
			x_alt = XOFFSET;
		else
		if(x_alt > (XOFFSET+XMAX) )
			x_alt = XOFFSET+YMAX;

		if(y_alt < YOFFSET)
			y_alt = YOFFSET;
		else
		if(y_alt > (YOFFSET+YMAX) )
			y_alt = YOFFSET+YMAX;
		
		ptr++;
		ptr++;

		for(m = 1; m <= n-1; m ++)	{
		
			if (*ptr & p)	{
				A = 0.9 * y_scale;
			}
			else	{
				A = 0;			
			}

			x =  x_zero + m * x_scale;
			y =  y_zero - o * y_scale - A;

			ptr++;
			ptr++;

			if(x < XOFFSET)
				x = XOFFSET;
			else
			if(x > (XOFFSET+XMAX) )
				x = XOFFSET+YMAX;

			if(y < YOFFSET)
				y = YOFFSET;
			else
			if(y > (YOFFSET+YMAX) )
				y = YOFFSET+YMAX;

			cairo_move_to(cr,x_alt,y_alt);
			cairo_line_to (cr,x,y);					
		
			x_alt = x;
			y_alt = y;
		}
		cairo_stroke(cr);	
	}

	y_zero = YOFFSET + YMAX / 2.0;

	for(o = 0;o <8; o++)
	{
		ptr = data->buffer;
		n = *data->length;

		ptr = ptr + 11;							//real data begins with offset + 10 + MSB
		n = (n - 10)/2;							//LA data are splited in to byte, LSB/MSB		
		
		p = (int) pow(2,o);

		if (*ptr & p)	{
			A = 0.9 * y_scale;
		}
		else	{
			A = 0;			
		}
	
		x_alt =  x_zero;
		y_alt =  y_zero - o * y_scale - A;

		if(x_alt < XOFFSET)
			x_alt = XOFFSET;
		else
		if(x_alt > (XOFFSET+XMAX) )
			x_alt = XOFFSET+YMAX;

		if(y_alt < YOFFSET)
			y_alt = YOFFSET;
		else
		if(y_alt > (YOFFSET+YMAX) )
			y_alt = YOFFSET+YMAX;
		
		ptr++;
		ptr++;

		for(m = 1; m <= n-1; m ++)	{
		
			if (*ptr & p)	{
				A = 0.9 * y_scale;
			}
			else	{
				A = 0;			
			}

			x =  x_zero + m * x_scale;
			y =  y_zero - o * y_scale - A;

			ptr++;
			ptr++;

			if(x < XOFFSET)
				x = XOFFSET;
			else
			if(x > (XOFFSET+XMAX) )
				x = XOFFSET+YMAX;

			if(y < YOFFSET)
				y = YOFFSET;
			else
			if(y > (YOFFSET+YMAX) )
				y = YOFFSET+YMAX;

			cairo_move_to(cr,x_alt,y_alt);
			cairo_line_to (cr,x,y);					
		
			x_alt = x;
			y_alt = y;
		}
		cairo_stroke(cr);	
	}

	
	if(*data->time_scale < 1E-6)
		sprintf(string_buf,"Time: %3.1f ns",*data->time_scale*1E9);	
	else
	if(*data->time_scale < 1E-3)
		sprintf(string_buf,"Time: %3.1f µs",*data->time_scale*1E6);
	else
	if(*data->time_scale < 1)
		sprintf(string_buf,"Time: %3.1f ms",*data->time_scale*1E3);
	else
		sprintf(string_buf,"Time: %3.1f s",*data->time_scale);

	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);			
	cairo_move_to (cr,XMAX/2, YMAX + 1.4 *YOFFSET);
	cairo_show_text(cr, string_buf); 	

	if(fabs(*data->time_offset) < 1E-3)
		sprintf(string_buf,"Time offset: %3.1f µs",*data->time_offset*1E6);
	else
	if(fabs(*data->time_offset) < 1)
		sprintf(string_buf,"Timet: %3.1f ms",*data->time_offset*1E3);
	else
		sprintf(string_buf,"Timet: %3.1f s",*data->time_offset);		
	
	cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
	cairo_move_to (cr,XMAX/2, YMAX + 1.7 *YOFFSET);
	cairo_show_text(cr, string_buf); 	
	
}

void find_DS1000(rigol_data *data) {
	#define MAX_RESPONCE_LENGTH 255
	char device[20]="";
	unsigned char buf [MAX_RESPONCE_LENGTH];
	int handle;
	int rc;
	int n;
	*data->device = -1;					//Default no device found

	for(n = 0; n < MAX_DEVICE_SEARCH; n++)	{
		sprintf(device,"/dev/usbtmc%d",n);
		#ifdef DEBUG
		g_print("%s\n",device);
		#endif
		handle=open(device, O_RDWR);
		if(handle>0) {
			rigol_write(handle, "*IDN?");
			rc=rigol_read(handle, buf,MAX_RESPONCE_LENGTH);
			#ifdef DEBUG	
			g_print("%s\n", buf);	
			#endif		
			if(rc>0)	{
				if (strstr(buf,"Rigol Technologies,DS10"))	{
					memcpy(data->device, &n, sizeof(n));				
					return;
				}
			}
		}	
	}
}	
