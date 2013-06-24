/* Editor hints for vim
 * vim:set ts=4 sw=4 noexpandtab:  */

/*
 * Copyright (C) 2013 DResearch Fahrzeugelektronik GmbH
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DRBCC_TRACE_H_
#define _DRBCC_TRACE_H_

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <drbcc.h> // definition of tracemask macros

#ifdef HAVE_LIBDRTRACE
drhip_trace_unit_t drbcc_trace_unit();
#define TRACE(mask,args...) DRHIP_TRACE(drbcc_trace_unit(), mask, DRHIP_TLVL_8, ##args)
#define TRACE_WARN(args...) DRHIP_TRACE_WARNING(drbcc_trace_unit(), ##args)
#else

#define TRACE(mask,f,args...)                                                       \
 	        if((libdrbcc_get_tracemask() >> (mask*4)) & 0xF)                        \
 	        {                                                                       \
 	                fprintf(stderr, "[libdrbcc:%08X] " f "\n", mask, ##args);       \
 	                fflush(stderr);                                                 \
 	        }
#define TRACE_WARN(format, args...) fprintf(stderr,"WARN :" format "\n", ##args)
unsigned long libdrbcc_get_tracemask();
void libdrbcc_set_tracemask(unsigned long t);
#endif

/*!\brief checked call
 *
 * prints a with an error message
 * when the call was not successful. Further it stores the return value of the
 * function call to the variable \c RVAL if and only if \c RVAL did not hold
 * another error value.
 *
 * \param MASK tracemask
 * \param RVAL the value where 
 * \param F the function to be called
 * \param ARGS a list of parameters passed to the function
 * \param OKVAL the return value treated as \e OK
 */
#define CHECKCALLX(MASK, RVAL, F, ARGS, OKVAL)							\
	{																	\
		const DRBCC_RC_t __rval = F ARGS;								\
		if ((OKVAL) != __rval)											\
		{																\
			if ((OKVAL) == (RVAL))										\
			{															\
				(RVAL) = __rval;										\
			}															\
			TRACE_WARN("%s%s failed with 0x%08X (%s)",		\
				  #F, #ARGS, __rval, drbcc_get_error_string(__rval));	\
		} else  {														\
			TRACE(MASK, "%s%s succeeded with 0x%08X (%s)",			\
				  #F, #ARGS, __rval, drbcc_get_error_string(__rval));	\
		}																\
	} 

/*!\brief checked call
 *
 * The same as \c CHECKCALLX , but \c OKVAL is assumed to be DRBCC_RC_NOERROR.
 *
 * \param MASK tracemask
 * \param RVAL the value where 
 * \param F the function to be called
 * \param ARGS a list of parameters passed to the function
 */
#define CHECKCALL(MASK, RVAL, F, ARGS) CHECKCALLX(MASK, RVAL, F, ARGS, DRBCC_RC_NOERROR)

#endif /* _DRBCC_TRACE_H_ */

/* Editor hints for emacs
 *
 * Local Variables:
 * mode:c
 * c-basic-offset:4
 * indent-tabs-mode:t
 * tab-width:4
 * End:
 * 
 * NO CODE BELOW THIS! */
