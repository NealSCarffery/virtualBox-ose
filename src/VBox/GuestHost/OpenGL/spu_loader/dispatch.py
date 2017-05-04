# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

from __future__ import print_function
import sys
import apiutil


apiutil.CopyrightC()

print("""

/* DO NOT EDIT - THIS FILE AUTOMATICALLY GENERATED BY dispatch.py SCRIPT */

#include "cr_spu.h"
#include "cr_string.h"
#include "cr_error.h"


static SPUGenericFunction __findFunc( char *name, SPU *spu )
{
	SPUNamedFunctionTable *temp;

	if (spu == NULL)
		return NULL;

	for (temp = spu->function_table->table ; temp->name != NULL ; temp++)
	{
		if (!crStrcmp(name, temp->name))
		{
			return temp->fn;
		}
	}
	return __findFunc(name, spu->superSPU);
}


/*
 * This function is not public outside the loader SPU.
 */
extern void __buildDispatch( SPU *spu );

void __buildDispatch( SPU *spu )
{""")

keys = apiutil.GetDispatchedFunctions(sys.argv[1]+"/APIspec.txt")
for func_name in keys:
	print('\tspu->dispatch_table.%s = (%sFunc_t) __findFunc( "%s", spu );' % (func_name,func_name,func_name))
print('}')


print("""

/*
 * Public function:
 * Search a SPU named function table for a specific function.  Return
 * a pointer to it or NULL if not found.
 */
SPUGenericFunction crSPUFindFunction( const SPUNamedFunctionTable *table, const char *fname )
{
	const SPUNamedFunctionTable *temp;

	for (temp = table ; temp->name != NULL ; temp++)
	{
		if (!crStrcmp(fname, temp->name))
		{
			return temp->fn;
		}
	}
	return NULL;
}


/*
 * Public function:
 * Initializes the pointers in an SPUDispatchTable by looking for functions
 * in an SPUNamedFunctionTable.
 * It doesn't know anything about SPUs and SPU inheritance.
 */
void crSPUInitDispatch( SPUDispatchTable *dispatch, const SPUNamedFunctionTable *table )
{""")

for func_name in keys:
	print('\tdispatch->%s = (%sFunc_t) crSPUFindFunction(table, "%s");' % (func_name, func_name, func_name))
print('}')



print("""
/*
 * Generic no-op function
 */
static int NopFunction(void)
{
/*
	crWarning("Calling generic no-op function in dispatch.c");
*/
	return 0;
}


/*
 * Scan the given dispatch table for NULL pointers.  Hook in the generic
 * no-op function wherever we find a NULL pointer.
 */
void crSPUInitDispatchNops(SPUDispatchTable *table)
{
	/*
	 * This is a bit tricky.  We walk over all the function pointers in
	 * the SPUDispatchTable struct, checking for NULL and setting NULL
	 * pointers to point to NopFunction().
	 * But we have to stop when we get to the copyList pointer!
	 */
	const int numEntries = (void **) &(table->copyList) - (void **) &(table->Accum);
	void **ptr = (void **) table;
	int i;
	for (i = 0; i < numEntries; i++) {
		if (ptr[i] == NULL) {
			/*printf("!!!!!!!Warning entry[%d] = NULL\n", i);*/
			ptr[i] = (void *)(uintptr_t)NopFunction;
		}
	}
}
""")
