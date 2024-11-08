/*
 * eon_http.h
 *
 *  Created on: Jun 26, 2023
 *      Author: hjx
 */

#ifndef EON_HTTP_H_
#define EON_HTTP_H_

#include "eon_net.h"

int EON_Http_Init(void);

int EON_Http_Open(EOTConnect* hConnect);
void EON_Http_Update(uint64_t tick);

#endif /* EON_HTTP_H_ */
