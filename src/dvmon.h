/* Editor hints for vim
 * vim:set ts=4 sw=4 noexpandtab:  */

/**\file
 * \brief  dvmon board controller com
 * \author Sascha Dierberg
 * 
 * (C) 2016 DResearch Digital Media Systems GmbH
 *
 * $Id$
 *
 */ 

int dvmon_init(const char* dev);

int dvmon(const char* cmd);

void dvmon_term();

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
