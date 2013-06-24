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


#ifndef _DRBCC_SEMA_
#define _DRBCC_SEMA_

typedef struct _drbcc_sema *DRBCC_sema_t;
 
// Functions

/*!\brief creates a semaphore 
 *
 * Creates a semaphore. 
 *
 * \param sempaphore pointer to DRBCC_sema_t
 * \return 1 on success, otherwise 0
 */
int drbcc_sema_create(DRBCC_sema_t *sema);

/*!\brief destroys a semaphore 
 *
 * Destroys a semapore. 
 *
 * \param semaphore pointer to DRBCC_sema_t
 */
void drbcc_sema_destroy(DRBCC_sema_t *sema);

/*!\brief Releases the semaphore. 
 *
 * Releases the semaphore. 
 *
 * \param sema DRBCC_sema_t semaphore to release
 * \return 1 on success, -1 if semaphore closed
 */
int drbcc_sema_release(DRBCC_sema_t sema);

/*!\brief Semaphore wait. 
 *
 * Wait until release is called or the semaphore was closed. 
 *
 * \param sema DRBCC_sema_t semaphore to wait
 * \return 1 on success, -1 if sema closed
 */
int drbcc_sema_wait(DRBCC_sema_t sema);

#endif /* _DRBCC_SEMA_ */

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
