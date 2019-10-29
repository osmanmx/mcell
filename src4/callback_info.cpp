
#include <iostream>
#include <assert.h>

#include "callback_info.h"

#ifndef NOSWIG
#include "Python.h"

using namespace std;

void py_callback_wall_hit(const MCell::WallHitInfo& res, void *clientdata)
{
   PyObject *func, *arglist;
   PyObject *result;
   double    dres = 0;

   func = (PyObject *) clientdata;               // Get Python function

   // transform into Python object
   //arglist = Py_BuildValue("(d)",a);             // Build argument list

   result = PyEval_CallObject(func, NULL/*arglist*/);     // Call Python
   //Py_DECREF(arglist);                           // Trash arglist
   if (result) {                                 // If no errors, return double
     dres = PyFloat_AsDouble(result);
   }
   Py_XDECREF(result);
   //return dres;
}


// check that our function pointer declaration matches
static __attribute__((unused)) wall_hit_callback_func check_wall_hit_decl = py_callback_wall_hit;

#endif