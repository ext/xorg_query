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
 * Parse a display string to a screen number.
 *
 * N     -> display, DefaultScreen
 * :N    -> display, DefaultScreen
 * N.M   -> display, screen
 * :N.M  -> display, screen
 *
 * @param zero if succesful.
 */
static int parse_screen(const char* display_string, int* display, int* screen){
	if ( !display_string ){
		return parse_screen(DisplayString(dpy), display, screen);
	}

	if ( display_string[0] == ':' ){
		display_string++; /* ignore : */
	}

	switch ( sscanf(display_string, "%d.%d", display, screen) ){
	case 2:
		return 0;
	case 1:
		*screen = DefaultScreen(dpy);
		return 0;
	default:
		return 1;
	}
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
	int display, screen, o;
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

	if ( !dpy ){
		PyErr_SetString(PyExc_RuntimeError, "Failed to open X Display");
		return NULL;
	}

	if ( parse_screen(string, &display, &screen) != 0 ){
		return PyErr_Format(PyExc_ValueError, "Failed to parse display `%s'", string);
	}

	root = XRootWindow(dpy, screen);
	if ( root == None ){
		PyErr_Format(PyExc_ValueError, "Can't open display: %s", string);
		return NULL;
	}

	XRRGetScreenSizeRange(dpy, root, &min_width, &min_height, &max_width, &max_height);
	res = XRRGetScreenResources (dpy, root);

	l = PyList_New(res->nmode);

	for ( o = 0; o < res->nmode; o++ ){
		PyList_SET_ITEM(l, o, Py_BuildValue("(iif)", res->modes[o].width, res->modes[o].height, mode_refresh(&res->modes[o])));
	}

	XRRFreeScreenResources(res);

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
