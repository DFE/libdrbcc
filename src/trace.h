/* Editor hints for vim
 * vim:set ts=4 sw=4 noexpandtab:  */

/*
 * Copyright (C) 2013 DResearch Fahrzeugelektronik GmbH
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _TRACE_H_
#define _TRACE_H_

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_LIBDRTRACE
#include <drhiptrace.h>
#endif


#ifdef HAVE_LIBDRTRACE
DRHIP_TRACE_UNIT_EXTERN(drbcctrace);
typedef enum
{
	TL_NONE  = DRHIP_TLVL_NONE, // to avoid compiler warning: comparison of unsigned expression >= 0 is always true 
	TL_INFO  = DRHIP_TLVL_1, // no debug info, just errors
	TL_DEBUG = DRHIP_TLVL_4, // + global info and warnings
	TL_TRACE = DRHIP_TLVL_8, // + mainloop activities
} tracelevel_t;
#else
typedef enum
{
	TL_NONE  = 0, // to avoid compiler warning: comparison of unsigned expression >= 0 is always true 
	TL_INFO  = 1, // no debug info, just errors
	TL_DEBUG = 4, // + global info and warnings
	TL_TRACE = 8, // + mainloop activities
} tracelevel_t;
#endif

#ifdef HAVE_LIBDRTRACE
#define TRACE(level,args...) DRHIP_TRACE(drbcctrace, DRHIP_TCAT_3, level, ##args)
#define TRACE_WARN(args...)  DRHIP_TRACE_WARNING(drbcctrace, ##args)
#else
#define TRACE(level,f,args...)         \
	if(level <= tracelevel())              \
	{                                      \
		fprintf(stderr, "[drbcc:%u] " f "\n", level, ##args);         \
		fflush(stderr);                    \
	}
#define TRACE_WARN(format, args...) fprintf(stderr,"WARN :" format "\n", ##args)
tracelevel_t tracelevel();
void set_tracelevel(tracelevel_t level);
#endif

/*!\brief checked call
 *
 * prints a with an error message
 * when the call was not successful. Further it stores the return value of the
 * function call to the variable \c RVAL if and only if \c RVAL did not hold
 * another error value.
 *
 * \param TLEV tracelevel
 * \param RVAL the value where 
 * \param F the function to be called
 * \param ARGS a list of parameters passed to the function
 * \param OKVAL the return value treated as \e OK
 */
#define CHECKCALLX(TLEV, RVAL, F, ARGS, OKVAL)						 \
{                                                                    \
	const DRBCC_RC_t __rval = F ARGS;                                \
	if ((OKVAL) != __rval)                                           \
	{                                                                \
		if ((OKVAL) == (RVAL))                                       \
		{                                                            \
			(RVAL) = __rval;                                         \
		}                                                            \
		TRACE_WARN("%s%s failed with 0x%08X (%s)",		 \
		        #F, #ARGS, __rval, drbcc_get_error_string(__rval));  \
	} else  {                                                        \
		TRACE(TLEV, "%s%s succeeded with 0x%08X (%s)",				 \
		        #F, #ARGS, __rval, drbcc_get_error_string(__rval));  \
	}                                                                \
} 

/*!\brief checked call
 *
 * The same as \c CHECKCALLX , but \c OKVAL is assumed to be DRBCC_RC_NOERROR.
 *
 * \param TLEV tracelevel
 * \param RVAL the value where 
 * \param F the function to be called
 * \param ARGS a list of parameters passed to the function
 */
#define CHECKCALL(TLEV, RVAL, F, ARGS) CHECKCALLX(TLEV, RVAL, F, ARGS, DRBCC_RC_NOERROR)

#endif /* _TRACE_H_ */

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
