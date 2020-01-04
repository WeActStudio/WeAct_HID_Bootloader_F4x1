/*
* STM32 HID Bootloader - USB HID bootloader for STM32F4X1
* Copyright (c) 2019 WeAct - WeAct_TC@163.com
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
*
* Modified From
* STM32 HID Bootloader - USB HID bootloader for STM32F10X
* Copyright (c) 2018 Bruno Freitas - bruno@brunofreitas.com
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "rs232.h"
#include "hidapi.h"

#include "hex2bin/readhex.h"

#define SECTOR_SIZE 1024
#define HID_TX_SIZE 65
#define HID_RX_SIZE 65

#define VID 1155
#define PID 22314
#define FIRMWARE_VER 0x0200

#define CMD_ResetPage (0x00)
#define CMD_Reboot    (0x01)
#define CMD_FW_Ver    (0x02)
#define CMD_Ack       (0x03)
#define CMD_Erase  	  (0x04)

uint8_t CMD_Base[6] = {'W','e','A','c','t',':'};

#ifndef WIN32

static void _split_whole_name(const char *whole_name, char *fname, char *ext)
{
	char *p_ext;
 
	p_ext = rindex(whole_name, '.');
	if (NULL != p_ext)
	{
		strcpy(ext, p_ext);
		snprintf(fname, p_ext - whole_name + 1, "%s", whole_name);
	}
	else
	{
		ext[0] = '\0';
		strcpy(fname, whole_name);
	}
}

void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
	char *p_whole_name;
 
	drive[0] = '\0';
	if (NULL == path)
	{
		dir[0] = '\0';
		fname[0] = '\0';
		ext[0] = '\0';
		return;
	}
 
	if ('/' == path[strlen(path)])
	{
		strcpy(dir, path);
		fname[0] = '\0';
		ext[0] = '\0';
		return;
	}
 
	p_whole_name = rindex(path, '/');
	if (NULL != p_whole_name)
	{
		p_whole_name++;
		_split_whole_name(p_whole_name, fname, ext);
 
		snprintf(dir, p_whole_name - path, "%s", path);
	}
	else
	{
		_split_whole_name(path, fname, ext);
		dir[0] = '\0';
	}
}
 
#endif

static int usb_write(hid_device *device, uint8_t *buffer, int len)
{
    int retries = 20;
    int retval;

    while (((retval = hid_write(device, buffer, len)) < len) && --retries)
    {
        if (retval < 0)
            // No data has been sent here. Delay and retry.
            usleep(100 * 1000); 
        else
            // Partial data has been sent. Firmware will be corrupted. Abort process.
            return 0; 
    }
    if (retries <= 0)
        return 0;

    return 1;
}

int serial_init(char *argument, uint8_t __timer)
{
    printf("> Trying to open the [%s] ...\n", argument);
    if (RS232_OpenComport(argument))
    {
        return (1);
    }
    printf("> Toggling DTR ...\n");

    RS232_disableRTS();
    RS232_enableDTR();
    usleep(200L);
    RS232_disableDTR();
    usleep(200L);
    RS232_enableDTR();
    usleep(200L);
    RS232_disableDTR();
    usleep(200L);
    RS232_send_magic();
    usleep(200L);
    RS232_CloseComport();

    printf("A %i\n",__timer);
    if (__timer > 0)
    {
        sleep(__timer);
    }
    return 0;
}

// CMD : 发送命令
// persize : HID 每次发送大小
int hid_sendCMD_NoResponse(hid_device *device,uint8_t cmd,size_t persize)
{
    uint8_t hid_tx_buf[persize];
    memset(hid_tx_buf, 0, sizeof(hid_tx_buf)); 
    hid_tx_buf[sprintf((char *)&hid_tx_buf[1],"WeAct:")+1] = cmd;
    // printf("%s",&hid_tx_buf[1]);

    if (!usb_write(device, hid_tx_buf, persize))
    {
        return 1;
    }
    return 0; 
}

// lenth : 发送长度
// persize : HID 每次发送大小64+1
int hid_sendData_NoResponse(hid_device *device,uint8_t *Data,size_t lenth,size_t persize)
{
    uint8_t hid_tx_buf[persize];
    
    for (int i = 0; i < lenth; i += persize - 1)
    {
        memset(hid_tx_buf, 0, sizeof(hid_tx_buf)); 
        memcpy(&hid_tx_buf[1], Data + i, HID_TX_SIZE - 1);
    }
}

// lenth 读取长度
// position 当前数组位置
// return 这次数据长度
size_t ReadData(const uint8_t *in,int insize,uint8_t *out,int lenth,int *position)
{
    int result;

    if(position[0] + lenth <= insize)
    {
        memcpy(out,&in[position[0]],lenth);
        position[0] = position[0] + lenth;
        result = lenth;
    }
    else
    {
        result = insize - position[0];
        memcpy(out,&in[position[0]],result);
        position[0] = insize;
    }
    // printf("> p:%d\n",position[0]);
    return result;
}

int main(int argc, char *argv[])
{
    uint8_t page_data[SECTOR_SIZE];
    uint8_t hid_tx_buf[HID_TX_SIZE];
    uint8_t hid_rx_buf[HID_RX_SIZE];

    // uint8_t CMD_RESET_PAGES[7] = {'W','e','A','c','t',':', 0x00}; // "WeAct: <0x00>"
    // uint8_t CMD_REBOOT_MCU[7] = {'W','e','A','c','t',':', 0x01};  // "WeAct: <0x01>"
    
    hid_device *handle = NULL;

    size_t read_bytes;
    FILE *firmware_file = NULL;

    int error = 0;
    uint32_t n_bytes = 0;
    int i;
    setbuf(stdout, NULL);
    
    uint8_t _timer = 0;

    printf("\n+-----------------------------------------------------------------------+\n");
    printf("|      WeAct HID-Flash Cli v1.0.0 - STM32 HID Bootloader Flash Tool     |\n");
    printf("|    Modified From HID-Flash v2.2.1 - STM32 HID Bootloader Flash Tool   |\n");
    printf("|              WeAct. Modified and Write by zhuyix 20191220             |\n");
    printf("+-----------------------------------------------------------------------+\n\n");
   
    // for(uint8_t i=0;i<argc;i++)
    //     printf("> %s",argv[i]);
    // printf("\n");

    if (argc < 2 )
    {
        printf("> Usage: WeAct_HID_Flash-CLI <bin/hex firmware_file> <reboot (optional)> : All CMD Used Except <read> <erase>\n");
        printf(">        WeAct_HID_Flash-CLI read                    : Just Read MCU whether Online And Information\n");
        printf(">        WeAct_HID_Flash-CLI <bin/hex firmware_file> : Just Download Firmware\n");
        printf(">        WeAct_HID_Flash-CLI reboot                  : Just Reboot MCU\n");
        printf(">        WeAct_HID_Flash-CLI erase                   : Just Erase MCU App\n");
        error = 1;
        exit(error);
        //return 1;
    }

    hid_init();
    printf("> Searching for HID Device [%04X:%04X] ...\n", VID, PID);

    struct hid_device_info *devs, *cur_dev;
    uint8_t valid_hid_devices = 0;

    printf("> ");
    for (i = 0; i < 10; i++)
    { 
        //Try up to 10 times to open the HID device.
        devs = hid_enumerate(VID, PID);
        cur_dev = devs;
        while (cur_dev)
        {  
            //Search for valid HID Bootloader USB devices
            if ((cur_dev->vendor_id == VID) && (cur_dev->product_id = PID))
            {
                valid_hid_devices++;
                if (cur_dev->release_number < FIRMWARE_VER)
                {  
                    //The STM32 board has firmware lower than 2.00
                    printf("\n> Error - Please update the firmware to the latest version");
                    goto exit;
                }
            }
            cur_dev = cur_dev->next;
        }
        hid_free_enumeration(devs);
        printf("**");
        usleep(500000);
        if (valid_hid_devices > 0)
            break;
    }

    if (valid_hid_devices == 0)
    {
        printf("\n> Error - HID Device [%04X:%04X] is not found\n", VID, PID);
        error = 1;
        goto exit;
    }

    handle = hid_open(VID, PID, NULL);

    if (i == 10 && handle != NULL)
    {
        printf("\n> Error - Unable to open the [%04X:%04X] device\n", VID, PID);
        error = 1;
        goto exit;
    }
    printf("\n> HID device [%04X:%04X] is found !\n",VID,PID);

    // 判断是否是复位
    if(strcmp(argv[1], "reboot") == 0)
    {
        goto restart_mcu;
    }
    // 判断是否是擦除 MCU APP
    else if(strcmp(argv[1], "erase") == 0)
    {
        // Send CMD_Erase_MCU command to reboot the microcontroller...
        printf("> Sending <erase mcu> command ...\n");
        
        memset(hid_tx_buf, 0, sizeof(hid_tx_buf));
        memcpy(&hid_tx_buf[1], CMD_Base, sizeof(CMD_Base));
        hid_tx_buf[1 + sizeof(CMD_Base)] = CMD_Erase;
        // Flash is unavailable when writing to it, so USB interrupt may fail here
        if (!usb_write(handle, hid_tx_buf, HID_TX_SIZE))
        {
            printf("> Error - While Sending <erase FW version> Command\n");
            error = 1;
            goto exit;
        }
        do
        {
            hid_read(handle, hid_rx_buf, 65);
            usleep(500);
        } while (memcmp(hid_rx_buf, CMD_Base, sizeof(CMD_Base)) != 0);
        printf("> MCU Erase Size: %3s\n", &hid_rx_buf[sizeof(CMD_Base)]);
        memset(hid_rx_buf, 0, sizeof(hid_rx_buf));
        
        // exit
        goto exit;
    }
    // 判断是否是读取版本信息
    else if(strcmp(argv[1], "read") == 0)
    {

        /* 读取固件版本 */
        printf("> Sending <read FW version> Command ...\n");
        //Fill the hid_tx_buf with zeros.
        memset(hid_tx_buf, 0, sizeof(hid_tx_buf)); 
        memcpy(&hid_tx_buf[1], CMD_Base, sizeof(CMD_Base));
        hid_tx_buf[1+sizeof(CMD_Base)] = CMD_FW_Ver;
        // Flash is unavailable when writing to it, so USB interrupt may fail here
        if (!usb_write(handle, hid_tx_buf, HID_TX_SIZE))
        {
            printf("> Error - While Sending <read FW version> Command\n");
            error = 1;
            goto exit;
        }
        do
        {
            hid_read(handle, hid_rx_buf, 65);
            // printf(". Ack %s\n", hid_rx_buf);
            usleep(500);
        } while (memcmp(hid_rx_buf,CMD_Base,sizeof(CMD_Base)) != 0);   
        printf("> FW version: %4s\n",&hid_rx_buf[sizeof(CMD_Base)]);
        memset(hid_rx_buf, 0, sizeof(hid_rx_buf));
        
        error = 0;
        goto exit;
    }
    // 判断是否是读取版本信息
    // if(strcmp(argv[1], "read") == 0)
    // {
    //     error = 0;
    //     goto exit;
    // }

    // 判断是否是固件地址
    // 。。。
    char drive[3];
    char dir[256];
    char fname[256];
    char ext[256];

    int size = 1024 * 1024;
    int offset = -1;
    unsigned char *b = malloc(size);

    if (b == NULL)
    {
        fprintf(stderr, "> Error - Couldn't Allocated %d bytes\n", size);
        exit(EXIT_FAILURE);
    }
    memset(b, '\377', size);

    struct memory_desc md;
    memory_desc_init(&md, b, offset, size);

    _splitpath(argv[1], drive, dir, fname, ext );
    printf("> Filename Extension: %s\n",ext);
    // 判断文件类型
    if((strcmp(ext,".bin") == 0) || (strcmp(ext,".hex") == 0) || \
        (strcmp(ext,".BIN") == 0) || (strcmp(ext,".HEX") == 0))
    {
        firmware_file = fopen(argv[1], "rb");
        if (!firmware_file)
        {
            printf("> Error - Opening Firmware File: %s\n", argv[1]);
            goto exit;
        }
        if((strcmp(ext,".hex") == 0) || (strcmp(ext,".HEX") == 0))
        {

            printf("> Hex To Bin ...\n");
            int success = read_hex(firmware_file, memory_desc_store, &md, 1);
            printf("> APP Size: %d bytes,success: %i,offset: 0x%lx\n",(uint32_t)md.size_written,success,md.offset);
            
            // goto exit;
        }
    }
    else
    {
        printf("> Error - Filename Extension No hex/bin: %s\n", argv[1]);
        goto exit;
    }
        

    /* Send RESET PAGES command to put HID bootloader in initial stage... */
    printf("> Sending <reset pages> command ...\n");

    if (hid_sendCMD_NoResponse(handle, CMD_ResetPage,HID_TX_SIZE) == 1)
    {
        printf(">  Error - While Sending <reset pages> Command\n");
        error = 1;
        goto exit;
    }

    /* Send Firmware File data */
    printf("> Flashing Firmware ...\n");
    memset(page_data, 0, sizeof(page_data));
    memset(hid_tx_buf, 0, sizeof(hid_tx_buf));

    int position = 0;

    if((strcmp(ext,".bin") == 0) || (strcmp(ext,".BIN") == 0))
        read_bytes = fread(page_data, 1, sizeof(page_data), firmware_file);
    else
    {
        read_bytes = ReadData(b,md.size_written,page_data,sizeof(page_data),&position);
        // printf("> read_bytes:%d\n",(uint32_t)read_bytes);
        // goto exit;
    }
    
    while (read_bytes > 0)
    {
        for (int i = 0; i < SECTOR_SIZE; i += HID_TX_SIZE - 1)
        {
            memcpy(&hid_tx_buf[1], page_data + i, HID_TX_SIZE - 1);

            if ((i % 1024) == 0)
            {
                printf(".");
            }

            // Flash is unavailable when writing to it, so USB interrupt may fail here
            if (!usb_write(handle, hid_tx_buf, HID_TX_SIZE))
            {
                printf("> Error - While Flashing Firmware Data\n");
                error = 1;
                goto exit;
            }
            n_bytes += (HID_TX_SIZE - 1);
            usleep(500);
        }

        printf(" %d Bytes\n", n_bytes);

        do
        {
            hid_read(handle, hid_rx_buf, 7);
            // printf(". Ack %6s0x%X\n",hid_rx_buf,hid_rx_buf[6]);
            sleep(0.5);
        } while (hid_rx_buf[6] != CMD_Ack);
        memset(hid_rx_buf, 0, sizeof(hid_rx_buf));

        memset(page_data, 0xFF, sizeof(page_data));
        // read_bytes = fread(page_data, 1, sizeof(page_data), firmware_file);

        if ((strcmp(ext, ".bin") == 0) || (strcmp(ext, ".BIN") == 0))
            read_bytes = fread(page_data, 1, sizeof(page_data), firmware_file);
        else
        {
            read_bytes = ReadData(b, md.size_written, page_data, sizeof(page_data), &position);
            // printf("> read_bytes:%d\n", (uint32_t)read_bytes);
            //goto exit;
        }
    }
    printf("> Flash Done !\n");

    if(argc == 2)
    {
        goto exit;
    }
    else if(argc == 3)
    {
        if(strcmp(argv[2], "reboot") != 0)
            goto exit;
    }

restart_mcu:

    // Send CMD_REBOOT_MCU command to reboot the microcontroller...
    printf("> Sending <reboot mcu> command ...\n");

    if (hid_sendCMD_NoResponse(handle, CMD_Reboot,HID_TX_SIZE) == 1)
    {
        printf("> Error - While Sending <reboot mcu> Command\n");
        error = 1;
        goto exit;
    }

exit:
    if (handle)
    {
        hid_close(handle);
    }

    hid_exit();

    if (firmware_file)
    {
        fclose(firmware_file);
    }

    if(b)
        free(b);

    printf("> Finish\n");

    exit(error);
    //return error;
}
