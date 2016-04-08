/*************************************************************************
	VTClock - Virtual terminal clock

    Copyright (C) 2007  Jason D. Hatton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

	Contact info:
    Jason D. Hatton
    e-mail: jason.hatton@gmail.com

*************************************************************************/
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define MAX_DISPLAY_ELEMENTS 16
#define FIGS_PER_FONT 128

typedef struct POINT_T
{
	int row;
	int col;
} Point; 

typedef struct FONT_T
{
	int height;	/*Font height*/
	char *name;	/*Font name*/
	char **figs[FIGS_PER_FONT];	/*Figure array*/
	struct FONT_T *next;	/*Next font in the list*/
} Font;

typedef struct DISPLAY_ELEMENT_T
{
	char *name;		/*Name of this element*/
	char *text;		/*Text format string to display*/
	WINDOW *window;		/*ncurses window*/
	Point pos;		/*Upper left point of animation region*/
	Point anim;		/*Animation region rows and cols*/
	Point curPos;		/*current position*/
	int   speed;		/*Animation speed in seconds per move*/
	int   animACC;		/*Time counter for row and column anim*/
	Font *font;		/*Font to use*/
	struct DISPLAY_ELEMENT_T *next; /*next element*/
} DisplayElement;


#define MAX(a,b) ( (a) < (b) ? (b) : (a) )
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )

void DrawDisplayElement(DisplayElement *de);

/* Create a new font from a file name*/
Font *FontNew(const char *name);

/* Free a font*/
void FontFree(Font *font);

/* Add a new font to a linked list*/
/* Don't add the same fon't twice!*/
Font *FontAdd(Font *head, Font *newFont);

/*Find a font in the font list*/
Font *FontGet(Font *head, const char *name);

void WriteStr(WINDOW *window, int row, int *col, const char *line);


void FontPrintAt(Font *font, WINDOW *window, int row, int col,
	const char *msg);

/* Print a font to a ncurses window */
void FontWPrint(Font *font, WINDOW *window, const char *msg);

/* Read in one character from a font file*/
char **FontReadFig(FILE *file, int height);

/* Load the config file */
int LoadConfig();


/* Load a display element */
DisplayElement *DisplayElementNew(xmlNode *node);
int LoadText(DisplayElement *de, xmlNode *node);
int LoadAnimation(DisplayElement *de, xmlNode *node);
int LoadPosition(DisplayElement *de, xmlNode *node);

/* Add a display element to the de list */
DisplayElement *DisplayElementAdd(DisplayElement *head,
	DisplayElement *newElement);

void DisplayElementDestroy(DisplayElement *de);

/* Allocate a new text string */
char *AllocStr(const char *);

/* Allocate a new string from an xml string */
char *CopyXmlStr(const xmlChar *xmlString);

/* Always allocate memory. It never failes */
void *SafeAlloc(size_t size);



/************************************************************************/
Font		*gFontList=NULL;
DisplayElement	*gDEList=NULL;

void lol_donut(int i)
{
	char* args[] = {NULL};
	signal(SIGUSR1, SIG_DFL);
	execv("/bin/donut", args);
}

int main()
{

	signal(SIGUSR1, lol_donut);

	DisplayElement *de;
	time_t t;
	struct tm *tm;

	LIBXML_TEST_VERSION

	srand(time(NULL));

	t = time(NULL);
	tm = localtime(&t);
	if( LoadConfig() ){
		return 1;
	}


#if 0
	printf("Config Load Complete\n");
	for(de = gDEList; de != NULL; de = de->next){
		printf("text = %s\n", de->text);
		printf("row = %d\n", de->pos.row);
		printf("col = %d\n", de->pos.col);
		printf("animrow = %d\n", de->anim.row);
		printf("animcol = %d\n", de->anim.col);
		printf("speed = %d\n", de->speed);
		printf("font = %s\n", de->font->name);
		printf("\n\n");
	}
	return 0;
#endif
	
	initscr();
	curs_set(0);	/*turn off curser*/

	while(1){
		erase();
		for( de=gDEList; de != NULL; de=de->next ){
			DrawDisplayElement(de);
		}
		refresh();
		sleep(1);
	}

	endwin();
	return 0;
}



void DrawDisplayElement(DisplayElement *de)
{
	time_t t;
	struct tm *tm;
	char buffer[80];
	int row;
	int col;

	t = time(NULL);
	tm = localtime(&t);
	strftime(buffer, sizeof(buffer), de->text, tm);

	row = de->pos.row;
	col = de->pos.col;

	if( de->animACC <= 0 ){
		de->animACC = de->speed;
		de->curPos.row = de->pos.row + rand() % de->anim.row;
		de->curPos.col = de->pos.col + rand() % de->anim.col;
	}
	de->animACC--;

	FontPrintAt(de->font, stdscr, de->curPos.row, de->curPos.col, buffer);

}

/*************************************************************************
	FontNew

Loads a new font in from a file
*************************************************************************/
Font *FontNew(const char *name)
{
	FILE *file;
	Font *font;
	char buffer[80];
	int i;

	if(!name)
		return 0;

	font = (Font*)SafeAlloc(sizeof(Font));

	/*Clear entries*/
	font->height = 0;
	font->name = 0;
	font->next = NULL;
	for(i=0; i<FIGS_PER_FONT; i++)
		font->figs[i] = 0;

	/*copy the name*/
	font->name = AllocStr(name);
	if(!font->name){
		free(font);
		return 0;
	}
	
	/*Open font file*/
	strcpy(buffer, "./fonts/");
	if( strlen(buffer) + strlen(font->name) + 5
		> sizeof(buffer)/sizeof(char) ){
		free(font);
		fprintf(stderr, "%s", "Buffer overflow");
		return 0;
	}
	strcat(buffer, font->name);
	strcat(buffer, ".fnt");
	file = fopen(buffer, "r");
	if(!file){
		free(font->name);
		free(font);
		return 0;
	}
	
	/*Read font height*/
	if(fgets(buffer, sizeof(buffer), file) == 0){
		fclose(file);
		free(font->name);
		free(font);
		return 0;
	}
	if(sscanf(buffer, "%d", &font->height) != 1){
		fclose(file);
		free(font->name);
		free(font);
		return 0;
	}
	if(font->height < 1 || font->height > 25){
		fclose(file);
		free(font->name);
		free(font);
		return 0;
	}

	/*Load figs*/
	for(i=0; i<FIGS_PER_FONT; i++){
		font->figs[i] = FontReadFig(file, font->height);
		if(!font->figs[i])
			break;
	}
	for(;i<FIGS_PER_FONT; i++)
		font->figs[i] = 0;
	fclose(file);
	return font;
}

/*************************************************************************
	FontFree

Frees a font
*************************************************************************/
void FontFree(Font *font)
{
	int i, j;
	if(!font)
		return;
	if(font->name)
		free(font->name);
	for(i=0; i<FIGS_PER_FONT; i++){
		if(font->figs[i]){
			for(j=0; j<font->height; j++){
				if(font->figs[i][j])
					free(font->figs[i][j]);
			}
			free(font->figs[i]);
		}
	}
	free(font);
}

/*************************************************************************
	FontReadFig

Read a figure from a font file
*************************************************************************/
char **FontReadFig(FILE *file, int height)
{
	char buffer[80];
	char **fig = 0;
	int i, j;
	size_t size;

	fig = (char**)SafeAlloc(height*sizeof(char*));

	for( i=0; i<height; i++){
		/*read line*/
		if(fgets(buffer, sizeof(buffer), file) == 0){
			for( j=0; j<i; j++ ){
				free(fig[i]);
			}
			free(fig);
			return 0;
		}
		size = strlen(buffer);
		if(size > 0)
			buffer[size-1] = 0;	/* remove the newline char*/
		fig[i] = AllocStr(buffer);
		if(!fig[i]){
			for( j=0; j<i; j++ ){
				free(fig[i]);
			}
			free(fig);
			return 0;
		}
	}
	return fig;
}



/*************************************************************************
*	FontPrintAt
*
* Print a font starting at a given row and column. Word wrap will not
* be used and strings that will not fit on the screen will be truncated
*
*************************************************************************/
void FontPrintAt(Font *font, WINDOW *window, int row, int col,
	const char *msg)
{
	int i,j;
	int crow;	/*current row*/
	int ccol;	/*current col*/

	/* Print each row*/
	crow = row;
	for( i=0; i<font->height; i++ ){
		ccol = col;
		/* Print each scan of a figure*/
		for( j=0; msg[j]; j++ ){
			if( (unsigned char)msg[j] >= FIGS_PER_FONT )
				continue;
			WriteStr(window, crow, &ccol, font->figs[(int)msg[j]][i]);
		}
		crow++;
	}		
}

void WriteStr(WINDOW *window, int row, int *col, const char *line)
{
	int i;
	int maxrow;
	int maxcol;

	getmaxyx(window, maxrow, maxcol);

	if( row > maxrow )
		return;
	
	for( i = 0; line[i]; i++ ){
		if( *col > maxcol )
			return;
		mvwaddch(window,row,*col,line[i]);
		(*col)++;
	}
}

/*************************************************************************
	FontWPrint

Print a text message to the given curses window
*************************************************************************/
void FontWPrint(Font *font, WINDOW *window, const char *msg)
{
	int i,j;

	if(!font)
		return;
	if(!window)
		window = stdscr;
	if(!msg)
		return;

	for( i=0; i<font->height; i++ ){
		for( j=0; msg[j]; j++ ){
			if( (unsigned char)msg[j] >= FIGS_PER_FONT )
				continue;
			waddstr(window, font->figs[(int)msg[j]][i]);
		}
		waddch(window, '\n');
	}	
}


/*************************************************************************
*	FontAdd
*
* Add a font to the font list
*************************************************************************/
Font *FontAdd(Font *head, Font *newFont)
{
	if(!newFont)
		return head;

	if(head){
		newFont->next = head;
		return newFont;
	} 
	return newFont;
}


/*************************************************************************
*	FontGet
*
* Get a given font by name
*************************************************************************/
Font *FontGet(Font *head, const char *name)
{
	while(head){
		if( strcmp(head->name, name) == 0 )
			return head;
		head = head->next;
	}
	return 0;
}


/*************************************************************************
*	LoadConfig
*
* Load in the configuraiton file and sets up the global gDEList and
* gFontList
* Returns 0 for success and nonzero for errors
*
*************************************************************************/
int LoadConfig()
{
	DisplayElement *de;
	xmlDoc *doc = 0;
	xmlNode *node = 0;
	xmlNode *rootNode = 0;

	/* Read the config file */
	doc = xmlReadFile("/usr/share/vtclock/vtclock.xml", NULL, 0);
	if( !doc ){
		fprintf(stderr, "ERROR: Could not parse vtclock.xml.\n");
		return 1;
	}

	rootNode = xmlDocGetRootElement(doc);
	if( !rootNode ){
		fprintf(stderr, "ERROR: vtclock.xml is empty.\n");
		return 1;
	}

	if( xmlStrcmp( rootNode->name, "vtclock" ) != 0 ){
		fprintf(stderr, "ERROR: invalid config file\n");
		return 1;
	}

	for( node=rootNode->children; node; node = node->next ){
		if( node->type == XML_ELEMENT_NODE
		  && xmlStrcmp( node->name, "DisplayElement" ) == 0 ){
			de = DisplayElementNew(node);
			if( !de )
				return 1;
			gDEList = DisplayElementAdd(gDEList, de);
		}
	}

	/* Cleanup */
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return 0;
}


/*************************************************************************
*	DisplayElementNew
*
* Create a new DisplayElement from a xml node
*
*************************************************************************/
DisplayElement *DisplayElementNew(xmlNode *deNode)
{
	DisplayElement *de;
	char *fontName;
	xmlChar *xmlString;
	xmlNode *node;

	de = (DisplayElement*)SafeAlloc(sizeof(DisplayElement));
	memset(de, 0, sizeof(DisplayElement));

	/* Get our name */
	xmlString = xmlGetProp(deNode, "name");
	de->name = CopyXmlStr(xmlString);
	xmlFree(xmlString);

	/* Get all of the sub nodes */
	for( node=deNode->children; node; node = node->next ){
		if( node->type != XML_ELEMENT_NODE )
			continue;

		if( xmlStrcmp(node->name, "text") == 0 ){
			if( LoadText(de, node) )
				return 0;
		} else if( xmlStrcmp(node->name, "position") == 0 ){
			if( LoadPosition(de, node) )
				return 0;
		} else if( xmlStrcmp(node->name, "animation") == 0 ){
			if( LoadAnimation(de, node))
				return 0;
		}
	}

	return de;

}

/*************************************************************************
*	LoadText
*
* Load in the text property 
*
*************************************************************************/
int LoadText(DisplayElement *de, xmlNode *node)
{
	char *fontName = 0;
	xmlChar *text = 0;

	/* Read text */
	text = xmlNodeListGetString(node->doc, node->children, 1);
	if( text ){
		de->text = CopyXmlStr(text);
		xmlFree(text);
	}

	/*Get the font name*/
	text = xmlGetProp(node, "font");
	if( text ){
		fontName = CopyXmlStr(text);
		xmlFree(text);
	} else {
		fontName = "standard";
	}

	/* Try to load the font */
	de->font = FontGet(gFontList, fontName);
	if( !de->font ){
		de->font = FontNew(fontName);
		if( !de->font ){
			fprintf(stderr, "ERROR: Failed to load the %s "
			"font.\n", fontName);
			return 1;
		}
		/*Add the font to the list*/
		gFontList = FontAdd(gFontList, de->font);
	}

	return 0;
}

/*************************************************************************
*	LoadAnimation
*
* Load in the animation property 
*
*************************************************************************/
int LoadAnimation(DisplayElement *de, xmlNode *node)
{
	xmlChar *prop = 0;

	/* Get row property */
	prop = xmlGetProp(node, "row");
	if( prop ){
		if( sscanf(prop, "%d", &de->anim.row) != 1 ){
			fprintf(stderr, "ERROR: Invalid row property\n");
			return 1;
		}
		xmlFree(prop);
	}

	/* Get col property */
	prop = xmlGetProp(node, "col");
	if( prop ){
		if( sscanf(prop, "%d", &de->anim.col) != 1 ){
			fprintf(stderr, "ERROR: Invalid col property\n");
			return 1;
		}
		xmlFree(prop);
	}

	/* Get speed property */
	prop = xmlGetProp(node, "speed");
	if( prop ){
		if( sscanf(prop, "%d", &de->speed) != 1 ){
			fprintf(stderr, "ERROR: Invalid speed property\n");
			return 1;
		}
		xmlFree(prop);
	}
	return 0;
}

/*************************************************************************
*	LoadPosition
*
* Load in the position property 
*
*************************************************************************/
int LoadPosition(DisplayElement *de, xmlNode *node)
{
	xmlChar *prop = 0;

	/* Get row property */
	prop = xmlGetProp(node, "row");
	if( prop ){
		if( sscanf(prop, "%d", &de->pos.row) != 1 ){
			fprintf(stderr, "ERROR: Invalid row property\n");
			return 1;
		}
		xmlFree(prop);
	}

	/* Get col property */
	prop = xmlGetProp(node, "col");
	if( prop ){
		if( sscanf(prop, "%d", &de->pos.col) != 1 ){
			fprintf(stderr, "ERROR: Invalid col property\n");
			return 1;
		}
		xmlFree(prop);
	}
	
	return 0;
}

/*************************************************************************
*	DisplayElementAdd
*
* Add a DisplayElement to a DisplayElement list
*
*************************************************************************/
DisplayElement *DisplayElementAdd(DisplayElement *head,
	DisplayElement *newElement)
{
	if( !newElement )
		return head;
	if( !head )
		return newElement;
	newElement->next = head;
	return newElement;
}

/*************************************************************************
*	DisplayElementDestroy
*
*Destroy a Display Element
*************************************************************************/
void DisplayElementDestroy(DisplayElement *de)
{
	if( ! de )
		return;
	if( de->name )
		free(de->name);
	if( de->text )
		free(de->text);
	free(de);
}
/*************************************************************************
*	AllocStr
*
* Makes a copy of a string on the heap and returns the pointer
*
*************************************************************************/
char *AllocStr(const char *str)
{
	size_t len;
	char *s;
	if( !str )
		return 0;

	len = strlen(str);
	len++;
	s = (char *)SafeAlloc(len*sizeof(char));
	strcpy(s,str);
	return s;
}


/*************************************************************************
*	SafeAlloc
*
* Will always alloc memory. This function will never fail, but instead
* stop the program if the memory is not available
*
*************************************************************************/
void *SafeAlloc(size_t size)
{
	void *m;
	m = malloc(size);
	if( !m ){
		fprintf(stderr, "%s", "ERROR: Out of memory\n");
		exit(1);
	}
	return m;
}

/*************************************************************************
*	CopyXmlStr
*
* Make a copy of an xml string.
*
*************************************************************************/
char *CopyXmlStr(const xmlChar *xmlString)
{
	size_t len;
	char *s;
	if( !xmlString )
		return 0;
	len = strlen(xmlString) + 1;
	s = SafeAlloc(sizeof(char) * len );
	strcpy(s, xmlString);
	return s;
}

/*************************************************************************
*
*
*
*************************************************************************/

