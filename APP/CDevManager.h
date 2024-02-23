/*
 * CDevManager.h
 *
 *  Created on: Dec 29, 2023
 *      Author: hjx
 */

#ifndef CDEVMANAGER_H_
#define CDEVMANAGER_H_

#include <stdint.h>

#include "CDevice.h"

void F_DevManager_Init(void);
void F_DevManager_Update(uint64_t tick);

#endif /* CDEVMANAGER_H_ */
