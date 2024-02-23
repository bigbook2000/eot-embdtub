/*
 * eob_flash.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#ifndef EOB_FLASH_H_
#define EOB_FLASH_H_

#define FLASH_SECTOR_0     			0U  /*!< Sector Number 0   */
#define FLASH_SECTOR_1     			1U  /*!< Sector Number 1   */
#define FLASH_SECTOR_2     			2U  /*!< Sector Number 2   */
#define FLASH_SECTOR_3     			3U  /*!< Sector Number 3   */
#define FLASH_SECTOR_4     			4U  /*!< Sector Number 4   */
#define FLASH_SECTOR_5     			5U  /*!< Sector Number 5   */
#define FLASH_SECTOR_6     			6U  /*!< Sector Number 6   */
#define FLASH_SECTOR_7     			7U  /*!< Sector Number 7   */
#define FLASH_SECTOR_8     			8U  /*!< Sector Number 8   */
#define FLASH_SECTOR_9     			9U  /*!< Sector Number 9   */
#define FLASH_SECTOR_10    			10U /*!< Sector Number 10  */
#define FLASH_SECTOR_11    			11U /*!< Sector Number 11  */

#define ADDRESS_FLASH_SECTOR_0		((unsigned int)0x08000000) 	/* Base @ of Sector 0, 16 Kbytes */
#define ADDRESS_FLASH_SECTOR_1     	((unsigned int)0x08004000) 	/* Base @ of Sector 1, 16 Kbytes */
#define ADDRESS_FLASH_SECTOR_2     	((unsigned int)0x08008000) 	/* Base @ of Sector 2, 16 Kbytes */
#define ADDRESS_FLASH_SECTOR_3     	((unsigned int)0x0800C000) 	/* Base @ of Sector 3, 16 Kbytes */
#define ADDRESS_FLASH_SECTOR_4     	((unsigned int)0x08010000) 	/* Base @ of Sector 4, 64 Kbytes */
#define ADDRESS_FLASH_SECTOR_5     	((unsigned int)0x08020000) 	/* Base @ of Sector 5, 128 Kbytes */
#define ADDRESS_FLASH_SECTOR_6     	((unsigned int)0x08040000) 	/* Base @ of Sector 6, 128 Kbytes */
#define ADDRESS_FLASH_SECTOR_7     	((unsigned int)0x08060000) 	/* Base @ of Sector 7, 128 Kbytes */
#define ADDRESS_FLASH_SECTOR_8     	((unsigned int)0x08080000) 	/* Base @ of Sector 8, 128 Kbytes */
#define ADDRESS_FLASH_SECTOR_9     	((unsigned int)0x080A0000) 	/* Base @ of Sector 9, 128 Kbytes */
#define ADDRESS_FLASH_SECTOR_10    	((unsigned int)0x080C0000) 	/* Base @ of Sector 10, 128 Kbytes */
#define ADDRESS_FLASH_SECTOR_11    	((unsigned int)0x080E0000) 	/* Base @ of Sector 11, 128 Kbytes */


// FLASH_SECTOR_0
int EOB_Flash_Erase(unsigned int nSector);

int EOB_Flash_Check(unsigned int nAddress, void* pBuffer, int nSize);
int EOB_Flash_Read(unsigned int nAddress, void* pBuffer, int nSize);
// 不检查，写之前先执行擦除，不处理跨扇区
int EOB_Flash_Write(unsigned int nAddress, void* pBuffer, int nSize);

#endif /* EOB_FLASH_H_ */
