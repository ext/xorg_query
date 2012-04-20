#include <Python.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>


static Display* dpy = NULL;

static float mode_refresh (XRRModeInfo *mode_info){
	if (mode_info->hTotal && mode_info->vTotal){
		return ((float) mode_info->dotClock / ((float) mode_info->hTotal * (float) mode_info->vTotal));
	} else {
		return 0;
	}
}

/**
 * Parse a display string to a screen number and return a normalized string.
 *
 * N     -> display, DefaultScreen
 * :N    -> display, DefaultScreen
 * N.M   -> display, screen
 * :N.M  -> display, screen
 *
 * @param display_string string or NULL for default.
 * @return NULL on errors and a normalized string if successful (pointing to static memory)
 * @bug Does not handle hostname.
 */
static const char* parse_screen(const char* display_string, int* display, int* screen){
	static char buffer[128];

	/* use default string */
	if ( !display_string ){
		return parse_screen(DisplayString(dpy), display, screen);
	}

	if ( display_string[0] == ':' ){
		display_string++; /* ignore : */
	}

	switch ( sscanf(display_string, "%d.%d", display, screen) ){
	case 2:
		break;
	case 1:
		*screen = DefaultScreen(dpy);
		break;
	default:
		return NULL;
	}

	/* create a normalized string */
	snprintf(buffer, sizeof(buffer), ":%d.%d", *display, *screen);
	return buffer;
}

static PyObject* display_string(){
	return Py_BuildValue("s", DisplayString(dpy));
}

static PyObject* query_screens(PyObject *self, PyObject *args) {
	PyObject* l = NULL;
	int n = 0, display = 0, screen = 0;
	char buf[16];

	if ( !dpy ){
		PyErr_SetString(PyExc_RuntimeError, "Failed to open X Display");
		return NULL;
	}

	parse_screen(NULL, &display, &screen);
	n = XScreenCount(dpy);
	l = PyList_New(n);

	for ( screen = 0; screen < n; screen++ ){
		snprintf(buf, 16, ":%d.%d", display, screen);
		PyList_SET_ITEM(l, screen, Py_BuildValue("s", buf));
	}

	return l;
}

static PyObject* query_resolution(PyObject *self, PyObject *args) {
	char* string = NULL;
	const char* normalized = NULL;
	int display, screen, o;
	Display* dpy;
	Window root;
	XRRScreenResources* res;
	PyObject* l;

	int min_width;
	int min_height;
	int max_width;
	int max_height;

	if ( !PyArg_ParseTuple(args, "|s", &string) ){
		return NULL;
	}

	/* parse the string to get screen number */
	if ( (normalized=parse_screen(string, &display, &screen)) == NULL ){
		return PyErr_Format(PyExc_ValueError, "Failed to parse display `%s'", string);
	}

	/* try to open the display */
	if ( (dpy=XOpenDisplay(normalized)) == NULL ){
		return PyErr_Format(PyExc_ValueError, "Can't open display: %s\n", normalized);
	}

	/* retrieve the screen sizes */
	root = XRootWindow(dpy, screen);
	XRRGetScreenSizeRange(dpy, root, &min_width, &min_height, &max_width, &max_height);
	res = XRRGetScreenResources (dpy, root);

	/* create a python list with the resolutions */
	l = PyList_New(res->nmode);
	for ( o = 0; o < res->nmode; o++ ){
		PyList_SET_ITEM(l, o, Py_BuildValue("(iif)", res->modes[o].width, res->modes[o].height, mode_refresh(&res->modes[o])));
	}

	/* free resources */
	XRRFreeScreenResources(res);
	XCloseDisplay(dpy);

	return l;
}

static PyObject* query_current_resolution(PyObject* self, PyObject* args, PyObject* kwargs) {
	char* string = NULL;
	char use_rotation;
	int display, screen, num_sizes;
	XRRScreenSize* xrrs;
	Rotation rotation;
	int width, height;

	static char* kwlist[] = {"screen", "use_rotation", NULL};
	if ( !PyArg_ParseTupleAndKeywords(args, kwargs, "|sb", kwlist, &string, &use_rotation) ){
		return NULL;
	}

	if ( !dpy ){
		PyErr_SetString(PyExc_RuntimeError, "Failed to open X Display");
		return NULL;
	}

	parse_screen(string, &display, &screen);
	xrrs = XRRSizes(dpy, screen, &num_sizes);
	XRRRotations(dpy, screen, &rotation);

	width = xrrs->width;
	height = xrrs->height;

	/* if the screen is rotated and use_rotation is True it will flip width and
	 * height so they reflect the rotated resolution.*/
	if ( use_rotation ){
		if ( rotation & (RR_Rotate_90|RR_Rotate_270) ){
			width = xrrs->height;
			height = xrrs->width;
		}
	}

	return Py_BuildValue("(ii)", width, height);
}

//Method table
static PyMethodDef QueryMethods[] = {
	{"display_string", display_string, METH_NOARGS, "Returns the string that was passed to XOpenDisplay when the current display was opened."},
	{"screens", query_screens, METH_VARARGS, "List all screens"},
	{"resolutions", query_resolution, METH_VARARGS, "List all resolutions for a screen. Defaults to current screen"},
	{"current_resolution", (PyCFunction)query_current_resolution,  METH_VARARGS|METH_KEYWORDS, "Get current resolution for a screen. Defaults to current screen"},
	{NULL, NULL, 0, NULL}
};

#if (PY_MAJOR_VERSION == 3)
static PyModuleDef QueryModule = {
    PyModuleDef_HEAD_INIT, "xorg_query", NULL, -1, QueryMethods,
    NULL, NULL, NULL, NULL
};
#endif

#if (PY_MAJOR_VERSION == 3)
#define INIT_FUNC_NAME PyInit_xorg_query
#else
#define INIT_FUNC_NAME initxorg_query
#endif

PyMODINIT_FUNC INIT_FUNC_NAME(void) {
	dpy = XOpenDisplay(NULL);

#if (PY_MAJOR_VERSION == 3)
	return PyModule_Create(&QueryModule);
#else
	Py_InitModule("xorg_query", QueryMethods);
#endif
}
