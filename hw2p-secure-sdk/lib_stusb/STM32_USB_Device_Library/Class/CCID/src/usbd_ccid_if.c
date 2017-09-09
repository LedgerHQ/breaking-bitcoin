/**
  ******************************************************************************
  * @file    usbd_ccid_if.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   This file provides all the functions for USB Interface for CCID 
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "usbd_ccid_if.h"

#ifdef HAVE_USB_CLASS_CCID

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint8_t Ccid_BulkState;
uint8_t UsbIntMessageBuffer[INTR_MAX_PACKET_SIZE];  /* data buffer*/
__IO uint8_t PrevXferComplete_IntrIn;
usb_ccid_param_t usb_ccid_param;

uint8_t* pUsbMessageBuffer;
static uint32_t UsbMessageLength;
Ccid_SlotStatus_t Ccid_SlotStatus;
Protocol0_DataStructure_t Protocol0_DataStructure;

Ccid_bulk_data_t Ccid_bulk_data;

/* Private function prototypes -----------------------------------------------*/
static void CCID_Response_SendData (USBD_HandleTypeDef  *pdev, 
                              uint8_t* pbuf, 
                              uint16_t len);
/* Private function ----------------------------------------------------------*/
/**
  * @brief  CCID_Init
  *         Initialize the CCID USB Layer
  * @param  pdev: device instance
  * @retval None
  */
void CCID_Init (USBD_HandleTypeDef  *pdev)
{
  memset(&Ccid_BulkState, 0, sizeof(Ccid_BulkState));
  memset(&UsbIntMessageBuffer, 0, sizeof(UsbIntMessageBuffer));
  memset(&PrevXferComplete_IntrIn, 0, sizeof(PrevXferComplete_IntrIn));
  memset(&usb_ccid_param, 0, sizeof(usb_ccid_param));
  memset(&pUsbMessageBuffer, 0, sizeof(pUsbMessageBuffer));
  memset(&UsbMessageLength, 0, sizeof(UsbMessageLength));
  memset(&Ccid_SlotStatus, 0, sizeof(Ccid_SlotStatus));
  memset(&Protocol0_DataStructure, 0, sizeof(Protocol0_DataStructure));
  memset(&Ccid_bulk_data, 0, sizeof(Ccid_bulk_data));

  /* CCID Related Initialization */
  CCID_SetIntrTransferStatus(1);  /* Transfer Complete Status */
  CCID_UpdSlotChange(1);
  SC_InitParams();  

  /* Prepare Out endpoint to receive 1st packet */ 
  Ccid_BulkState = CCID_STATE_IDLE;
  USBD_LL_PrepareReceive(pdev, CCID_BULK_OUT_EP, CCID_BULK_EPOUT_SIZE);
}

/**
  * @brief  CCID_DeInit
  *         Uninitialize the CCID Machine
  * @param  pdev: device instance
  * @retval None
  */
void CCID_DeInit (USBD_HandleTypeDef  *pdev)
{
   UNUSED(pdev);
   Ccid_BulkState = CCID_STATE_IDLE;
}

/**
  * @brief  CCID_Message_In
  *         Handle Bulk IN & Intr IN data stage 
  * @param  pdev: device instance
  * @param  uint8_t epnum: endpoint index
  * @retval None
  */
void CCID_BulkMessage_In (USBD_HandleTypeDef  *pdev, 
                     uint8_t epnum)
{  
  if (epnum == (CCID_BULK_IN_EP & 0x7F))
  {/* Filter the epnum by masking with 0x7f (mask of IN Direction)  */
    
    /*************** Handle Bulk Transfer IN data completion  *****************/
    
    switch (Ccid_BulkState)
    {
    case CCID_STATE_SEND_RESP: {
      unsigned int remLen = UsbMessageLength;

      // advance with acknowledged sent chunk
      pUsbMessageBuffer += MIN(CCID_BULK_EPIN_SIZE, UsbMessageLength);
      UsbMessageLength -= MIN(CCID_BULK_EPIN_SIZE, UsbMessageLength);

      // if remaining length is > EPIN_SIZE: send a filled bulk packet
      if (UsbMessageLength >= CCID_BULK_EPIN_SIZE) {
        CCID_Response_SendData(pdev, pUsbMessageBuffer, 
                                      // use the header declared size packet must be well formed
                                      CCID_BULK_EPIN_SIZE);
      }

      // if remaining length is 0; send an empty packet and prepare to receive a new command
      else if (UsbMessageLength == 0 && remLen == CCID_BULK_EPIN_SIZE) {
        CCID_Response_SendData(pdev, pUsbMessageBuffer, 
                                      // use the header declared size packet must be well formed
                                      0);
        goto last_xfer; // won't wait ack to avoid missing a command
      }
      // else if no more data, then last packet sent, go back to idle (done on transfer ack)
      else if (UsbMessageLength == 0) { // robustness only
      last_xfer:
        Ccid_BulkState = CCID_STATE_IDLE;
        
        /* Prepare EP to Receive First Cmd */
        USBD_LL_PrepareReceive(pdev, CCID_BULK_OUT_EP, CCID_BULK_EPOUT_SIZE);
      }

      // if remaining length is < EPIN_SIZE: send packet and prepare to receive a new command
      else if (UsbMessageLength < CCID_BULK_EPIN_SIZE) {
        CCID_Response_SendData(pdev, pUsbMessageBuffer, 
                                      // use the header declared size packet must be well formed
                                      UsbMessageLength);
        goto last_xfer; // won't wait ack to avoid missing a command
      }

      break;
    }
      
    default:
      break;
    }
  }
  else if (epnum == (CCID_INTR_IN_EP & 0x7F))
  {
    /* Filter the epnum by masking with 0x7f (mask of IN Direction)  */
    CCID_SetIntrTransferStatus(1);  /* Transfer Complete Status */
  }
}

/**
  * @brief  CCID_BulkMessage_Out
  *         Proccess CCID OUT data
  * @param  pdev: device instance
  * @param  uint8_t epnum: endpoint index
  * @retval None
  */
void CCID_BulkMessage_Out (USBD_HandleTypeDef  *pdev, 
                           uint8_t epnum, uint8_t* buffer, uint16_t dataLen)
{
   
  switch (Ccid_BulkState)
  {
  case CCID_STATE_IDLE:
    if (dataLen == 0x00)
    { /* Zero Length Packet Received */
      Ccid_BulkState = CCID_STATE_IDLE;
    }
    else  if (dataLen >= CCID_MESSAGE_HEADER_SIZE)
    {
      UsbMessageLength = dataLen;   /* Store for future use */
      
      /* Expected Data Length Packet Received */
      pUsbMessageBuffer = (uint8_t*) &Ccid_bulk_data;
      
      /* Fill CCID_BulkOut Data Buffer from USB Buffer */
      memmove(pUsbMessageBuffer, buffer, dataLen); 
      
      /*
      Refer : 6 CCID Messages
      The response messages always contain the exact same slot number, 
      and sequence number fields from the header that was contained in 
      the Bulk-OUT command message.
      */
      Ccid_bulk_data.header.bulkin.bSlot = Ccid_bulk_data.header.bulkout.bSlot; 
      Ccid_bulk_data.header.bulkin.bSeq = Ccid_bulk_data.header.bulkout.bSeq;
      
      if (dataLen < CCID_BULK_EPOUT_SIZE)
      {/* Short message, less than the EP Out Size, execute the command,
        if parameter like dwLength is too big, the appropriate command will 
        give an error */
        CCID_CmdDecode(pdev);  
      }
      else
      { /* Long message, receive additional data with command */
        /* (u8dataLen == CCID_BULK_EPOUT_SIZE) */
        
        if (Ccid_bulk_data.header.bulkout.dwLength > ABDATA_SIZE)
        { /* Check if length of data to be sent by host is > buffer size */
          
          /* Too long data received.... Error ! */
          Ccid_BulkState = CCID_STATE_UNCORRECT_LENGTH;
        }
        
        else 
        { /* Expect more data on OUT EP */
          Ccid_BulkState = CCID_STATE_RECEIVE_DATA;
          pUsbMessageBuffer += dataLen;  /* Point to new offset */      
          
          /* Prepare EP to Receive next Cmd */
          USBD_LL_PrepareReceive(pdev, CCID_BULK_OUT_EP, CCID_BULK_EPOUT_SIZE);
          
        } /* if (dataLen == CCID_BULK_EPOUT_SIZE) ends */
      } /*  if (dataLen >= CCID_BULK_EPOUT_SIZE) ends */
    } /* if (dataLen >= CCID_MESSAGE_HEADER_SIZE) ends */
    break;
    
  case CCID_STATE_RECEIVE_DATA:
    
    UsbMessageLength += dataLen;
    
    if (dataLen < CCID_BULK_EPOUT_SIZE)
    {/* Short message, less than the EP Out Size, execute the command,
        if parameter like dwLength is too big, the appropriate command will 
        give an error */
      
      /* Full command is received, process the Command */
      memmove(pUsbMessageBuffer, buffer, dataLen); 
      CCID_CmdDecode(pdev); 
    }
    else if (dataLen == CCID_BULK_EPOUT_SIZE)
    {  
      if (UsbMessageLength < (Ccid_bulk_data.header.bulkout.dwLength + CCID_CMD_HEADER_SIZE))
      {
        memmove(pUsbMessageBuffer, buffer, dataLen); 
        pUsbMessageBuffer += dataLen; 
        /* Increment the pointer to receive more data */
        
        /* Prepare EP to Receive next Cmd */
        USBD_LL_PrepareReceive(pdev, CCID_BULK_OUT_EP, CCID_BULK_EPOUT_SIZE);
      }
      else if (UsbMessageLength == (Ccid_bulk_data.header.bulkout.dwLength + CCID_CMD_HEADER_SIZE))
      { 
        /* Full command is received, process the Command */
        memmove(pUsbMessageBuffer, buffer, dataLen); 
        CCID_CmdDecode(pdev);
      }
      else
      {
        /* Too long data received.... Error ! */
        Ccid_BulkState = CCID_STATE_UNCORRECT_LENGTH;
      }
    }
    
    break;
  
  case CCID_STATE_UNCORRECT_LENGTH:
    Ccid_BulkState = CCID_STATE_IDLE;
    break;
  
  default:
    break;
  }
}

void CCID_Send_Reply(USBD_HandleTypeDef  *pdev) {
  /********** Decide for all commands ***************/ 
  if (Ccid_BulkState == CCID_STATE_SEND_RESP)
  {
    UsbMessageLength = Ccid_bulk_data.header.bulkin.dwLength+CCID_MESSAGE_HEADER_SIZE;   /* Store for future use */
      
    /* Expected Data Length Packet Received */
    pUsbMessageBuffer = (uint8_t*) &Ccid_bulk_data;

    CCID_Response_SendData(pdev, pUsbMessageBuffer, 
                                  // use the header declared size packet must be well formed
                                  MIN(CCID_BULK_EPIN_SIZE, UsbMessageLength));
  }
}

/**
  * @brief  CCID_CmdDecode
  *         Parse the commands and Proccess command
  * @param  pdev: device instance
  * @retval None
  */
void CCID_CmdDecode(USBD_HandleTypeDef  *pdev)
{
  uint8_t errorCode;
  
  switch (Ccid_bulk_data.header.bulkout.bMessageType)
  {
  case PC_TO_RDR_ICCPOWERON:
    errorCode = PC_to_RDR_IccPowerOn();
    RDR_to_PC_DataBlock(errorCode);
    break;
  case PC_TO_RDR_ICCPOWEROFF:
    errorCode = PC_to_RDR_IccPowerOff();
    RDR_to_PC_SlotStatus(errorCode);
    break;
  case PC_TO_RDR_GETSLOTSTATUS:
    errorCode = PC_to_RDR_GetSlotStatus();
    RDR_to_PC_SlotStatus(errorCode);
    break;
  case PC_TO_RDR_XFRBLOCK:
    errorCode = PC_to_RDR_XfrBlock();
    // asynchronous // RDR_to_PC_DataBlock(errorCode);
    break;
  case PC_TO_RDR_GETPARAMETERS:
    errorCode = PC_to_RDR_GetParameters();
    RDR_to_PC_Parameters(errorCode);
    break;
  case PC_TO_RDR_RESETPARAMETERS:
    errorCode = PC_to_RDR_ResetParameters();
    RDR_to_PC_Parameters(errorCode);
    break;
  case PC_TO_RDR_SETPARAMETERS:
    errorCode = PC_to_RDR_SetParameters();
    RDR_to_PC_Parameters(errorCode);
    break;
  case PC_TO_RDR_ESCAPE:
    errorCode = PC_to_RDR_Escape();
    RDR_to_PC_Escape(errorCode);
    break;
  case PC_TO_RDR_ICCCLOCK:
    errorCode = PC_to_RDR_IccClock();
    RDR_to_PC_SlotStatus(errorCode);
    break;
  case PC_TO_RDR_ABORT:
    errorCode = PC_to_RDR_Abort();
    RDR_to_PC_SlotStatus(errorCode);
    break;
  case PC_TO_RDR_T0APDU:
    errorCode = PC_TO_RDR_T0Apdu();
    RDR_to_PC_SlotStatus(errorCode);
    break;
  case PC_TO_RDR_MECHANICAL:
    errorCode = PC_TO_RDR_Mechanical();
    RDR_to_PC_SlotStatus(errorCode);
    break;   
  case PC_TO_RDR_SETDATARATEANDCLOCKFREQUENCY:
    errorCode = PC_TO_RDR_SetDataRateAndClockFrequency();
    RDR_to_PC_DataRateAndClockFrequency(errorCode);
    break;
  case PC_TO_RDR_SECURE:
    errorCode = PC_TO_RDR_Secure();
    RDR_to_PC_DataBlock(errorCode);
    break;
  default:
    RDR_to_PC_SlotStatus(SLOTERROR_CMD_NOT_SUPPORTED);
    break;
  }
  
  CCID_Send_Reply(pdev);
}

/**
  * @brief  Transfer_Data_Request
  *         Prepare the request response to be sent to the host
  * @param  uint8_t* dataPointer: Pointer to the data buffer to send
  * @param  uint16_t dataLen : number of bytes to send
  * @retval None
  */
void Transfer_Data_Request(void)
{
   /**********  Update Global Variables ***************/
   Ccid_BulkState = CCID_STATE_SEND_RESP;    
}   
  
  
/**
  * @brief  CCID_Response_SendData
  *         Send the data on bulk-in EP 
  * @param  pdev: device instance
  * @param  uint8_t* buf: pointer to data buffer
  * @param  uint16_t len: Data Length
  * @retval None
  */
static void  CCID_Response_SendData(USBD_HandleTypeDef  *pdev,
                              uint8_t* buf, 
                              uint16_t len)
{  
    // don't ask the MCU to perform bulk split, we could quickly get into a buffer overflow
    if (len > CCID_BULK_EPIN_SIZE) {
      THROW(EXCEPTION_IO_OVERFLOW);
    }

    G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_USB_EP_PREPARE;
    G_io_seproxyhal_spi_buffer[1] = (3+len)>>8;
    G_io_seproxyhal_spi_buffer[2] = (3+len);
    G_io_seproxyhal_spi_buffer[3] = CCID_BULK_IN_EP;
    G_io_seproxyhal_spi_buffer[4] = SEPROXYHAL_TAG_USB_EP_PREPARE_DIR_IN;
    G_io_seproxyhal_spi_buffer[5] = len;
    io_seproxyhal_spi_send(G_io_seproxyhal_spi_buffer, 6);
    io_seproxyhal_spi_send(buf, len);
}

/**
  * @brief  CCID_IntMessage
  *         Send the Interrupt-IN data to the host
  * @param  pdev: device instance
  * @retval None
  */
void CCID_IntMessage(USBD_HandleTypeDef  *pdev)
{
  /* Check if there us change in Smartcard Slot status */  
  if ( CCID_IsSlotStatusChange() && CCID_IsIntrTransferComplete() )
  {
    /* Check Slot Status is changed. Card is Removed/ Fitted  */
    RDR_to_PC_NotifySlotChange();
    
    CCID_SetIntrTransferStatus(0);  /* Reset the Status */
    CCID_UpdSlotChange(0);    /* Reset the Status of Slot Change */
    
    G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_USB_EP_PREPARE;
    G_io_seproxyhal_spi_buffer[1] = (3+2)>>8;
    G_io_seproxyhal_spi_buffer[2] = (3+2);
    G_io_seproxyhal_spi_buffer[3] = CCID_INTR_IN_EP;
    G_io_seproxyhal_spi_buffer[4] = SEPROXYHAL_TAG_USB_EP_PREPARE_DIR_IN;
    G_io_seproxyhal_spi_buffer[5] = 2;
    io_seproxyhal_spi_send(G_io_seproxyhal_spi_buffer, 6);
    io_seproxyhal_spi_send(UsbIntMessageBuffer, 2);
  }
}  

/**
  * @brief  CCID_IsIntrTransferComplete
  *         Provides the status of previous Interrupt transfer status
  * @param  None 
  * @retval uint8_t PrevXferComplete_IntrIn: Value of the previous transfer status
  */
uint8_t CCID_IsIntrTransferComplete (void)
{
  return PrevXferComplete_IntrIn;
}

/**
  * @brief  CCID_IsIntrTransferComplete
  *         Set the value of the Interrupt transfer status 
  * @param  uint8_t xfer_Status: Value of the Interrupt transfer status to set
  * @retval None 
  */
void CCID_SetIntrTransferStatus (uint8_t xfer_Status)
{
  PrevXferComplete_IntrIn = xfer_Status;
}






uint8_t SC_Detect(void) {
  return 1;
}

void SC_Poweroff(void) {
  // nothing to do

}

void SC_InitParams (void) {
  // nothing to do
}

uint8_t SC_SetParams (Protocol0_DataStructure_t* pt0) {
  return SLOT_NO_ERROR;
}

uint8_t SC_ExecuteEscape (uint8_t* escapePtr, uint32_t escapeLen, 
                          uint8_t* responseBuff,
                          uint16_t* responseLen) {
  io_seproxyhal_se_reset();
}
uint8_t SC_SetClock (uint8_t bClockCommand) {
  return SLOT_NO_ERROR;
}
uint8_t SC_Request_GetClockFrequencies(uint8_t* pbuf, uint16_t* len);
uint8_t SC_Request_GetDataRates(uint8_t* pbuf, uint16_t* len);
uint8_t SC_T0Apdu(uint8_t bmChanges, uint8_t bClassGetResponse, 
                  uint8_t bClassEnvelope) {
  return SLOTERROR_CMD_NOT_SUPPORTED;
}
uint8_t SC_Mechanical(uint8_t bFunction) {
  return SLOTERROR_CMD_NOT_SUPPORTED;
}
uint8_t SC_SetDataRateAndClockFrequency(uint32_t dwClockFrequency, 
                                        uint32_t dwDataRate) {
  return SLOT_NO_ERROR;
}
uint8_t SC_Secure(uint32_t dwLength, uint8_t bBWI, uint16_t wLevelParameter, 
                    uint8_t* pbuf, uint32_t* returnLen )  {
  return SLOTERROR_CMD_NOT_SUPPORTED;
}

// prepare the apdu to be processed by the application
uint8_t SC_XferBlock (uint8_t* ptrBlock, uint32_t blockLen, uint16_t* expectedLen) {
  // check for overflow
  if (blockLen > IO_APDU_BUFFER_SIZE) {
    return SLOTERROR_BAD_LENTGH;
  }
  
  // copy received apdu
  memmove(G_io_apdu_buffer, ptrBlock, blockLen);
  G_io_apdu_length = blockLen;
  G_io_apdu_media = IO_APDU_MEDIA_USB_CCID; // for application code
  G_io_apdu_state = APDU_USB_CCID; // for next call to io_exchange

  return SLOT_NO_ERROR;
}

void io_usb_ccid_reply(unsigned char* buffer, unsigned short length) {
  // avoid memory overflow
  if (length > sizeof(Ccid_bulk_data.abData)) {
    THROW(EXCEPTION_IO_OVERFLOW);
  }
  // copy the responde apdu
  memmove(Ccid_bulk_data.abData, buffer, length);
  Ccid_bulk_data.header.bulkin.dwLength = length;
  // forge reply
  RDR_to_PC_DataBlock(SLOT_NO_ERROR);

  // start sending rpely
  CCID_Send_Reply(&USBD_Device);
}

// ask for power on
void io_usb_ccid_poweron(void) {
  CCID_UpdSlotChange(1);
  CCID_IntMessage(&USBD_Device);
}







#endif // HAVE_USB_CLASS_CCID

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
