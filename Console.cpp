////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

#include "Console.h"
#include "Commands.h"
#include "Scheduler.h"

//// /// @fn int consolePrintMessage(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage, const char *message)
//// ///
//// /// @brief Print a message to all console ports that are owned by a process.
//// ///
//// /// @param consoleState The ConsoleState being maintained by the runConsole
//// ///   function.
//// /// @param inputMessage The message received from the process printing the
//// ///   message.
//// /// @param message The formatted string message to print.
//// ///
//// /// @return Returns processSuccess on success, processError on failure.
//// int consolePrintMessage(
////   ConsoleState *consoleState, ProcessMessage *inputMessage, const char *message
//// ) {
////   int returnValue = processSuccess;
////   ProcessId owner = processId(processMessageFrom(inputMessage));
////   ConsolePort *consolePorts = consoleState->consolePorts;
//// 
////   bool portFound = false;
////   for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     if (consolePorts[ii].outputOwner == owner) {
////       consolePorts[ii].printString(message);
////       portFound = true;
////     }
////   }
//// 
////   if (portFound == false) {
////     printString("WARNING:  Request to print message \"");
////     printString(message);
////     printString("\" from non-owning process ");
////     printInt(owner);
////     printString("\n");
////     returnValue = processError;
////   }
//// 
////   return returnValue;
//// }
//// 
//// /// @fn void consoleMessageCleanup(ProcessMessage *inputMessage)
//// ///
//// /// @brief Release an input ProcessMessage if there are no waiters for the message.
//// ///
//// /// @param inputMessage A pointer to the ProcessMessage to cleanup.
//// ///
//// /// @return This function returns no value.
//// void consoleMessageCleanup(ProcessMessage *inputMessage) {
////   if (processMessageWaiting(inputMessage) == false) {
////     if (processMessageRelease(inputMessage) != processSuccess) {
////       Serial.print("ERROR!!!  Could not release inputMessage from ");
////       Serial.print(__func__);
////       Serial.print("\n");
////     }
////   }
//// }
//// 
//// /// @fn ConsoleBuffer* getAvailableConsoleBuffer(
//// ///   ConsoleState *consoleState, ProcessId processId)
//// ///
//// /// @brief Get an available console buffer and mark it as being in use.
//// ///
//// /// @param consoleState A pointer to the ConsoleState structure held by the
//// ///   runConsole process.  The buffers in this state will be searched for
//// /// @param processId The numerical ID of the process (PID) making the request
//// ///   for a buffer (if any).
//// ///
//// /// @return Returns a pointer to the available ConsoleBuffer on success, NULL
//// /// on failure.
//// ConsoleBuffer* getAvailableConsoleBuffer(
////   ConsoleState *consoleState, ProcessId processId
//// ) {
////   ConsoleBuffer *consoleBuffers = consoleState->consoleBuffers;
////   ConsoleBuffer *returnValue = NULL;
//// 
////   // Check to see if the requesting process owns one of the ports for output.
////   // Use the buffer for that port if so.
////   ConsolePort *consolePorts = consoleState->consolePorts;
////   for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     if ((consolePorts[ii].outputOwner == processId)
////       || (consolePorts[ii].inputOwner == processId)
////     ) {
////       returnValue = &consoleBuffers[ii];
////       // returnValue->inUse is already set to true, so no need to set it.
////       break;
////     }
////   }
//// 
////   if (returnValue == NULL) {
////     returnValue = (ConsoleBuffer*) malloc(sizeof(ConsoleBuffer));
////   }
//// 
////   return returnValue;
//// }
//// 
//// /// @fn void consoleWriteValueCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Command handler for the CONSOLE_WRITE_VALUE command.
//// ///
//// /// @param consoleState A pointer to the ConsoleState structure held by the
//// ///   runConsole process.  Unused by this function.
//// /// @param inputMessage A pointer to the ProcessMessage that was received from
//// ///   the process that sent the command.
//// ///
//// /// @return This function returns no value but does set the inputMessage to
//// /// done so that the calling process knows that we've handled the message.
//// void consoleWriteValueCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   char staticBuffer[19]; // max length of a 64-bit value is 18 digits plus NULL.
////   ConsoleValueType valueType
////     = nanoOsMessageFuncValue(inputMessage, ConsoleValueType);
////   const char *message = NULL;
//// 
////   switch (valueType) {
////     case CONSOLE_VALUE_CHAR:
////       {
////         char value = nanoOsMessageDataValue(inputMessage, char);
////         sprintf(staticBuffer, "%c", value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_UCHAR:
////       {
////         unsigned char value
////           = nanoOsMessageDataValue(inputMessage, unsigned char);
////         sprintf(staticBuffer, "%u", value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_INT:
////       {
////         int value = nanoOsMessageDataValue(inputMessage, int);
////         sprintf(staticBuffer, "%d", value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_UINT:
////       {
////         unsigned int value
////           = nanoOsMessageDataValue(inputMessage, unsigned int);
////         sprintf(staticBuffer, "%u", value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_LONG_INT:
////       {
////         long int value = nanoOsMessageDataValue(inputMessage, long int);
////         sprintf(staticBuffer, "%ld", value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_LONG_UINT:
////       {
////         long unsigned int value
////           = nanoOsMessageDataValue(inputMessage, long unsigned int);
////         sprintf(staticBuffer, "%lu", value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_FLOAT:
////       {
////         float value = nanoOsMessageDataValue(inputMessage, float);
////         sprintf(staticBuffer, "%f", (double) value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_DOUBLE:
////       {
////         double value = nanoOsMessageDataValue(inputMessage, double);
////         sprintf(staticBuffer, "%lf", value);
////         message = staticBuffer;
////       }
////       break;
//// 
////     case CONSOLE_VALUE_STRING:
////       {
////         message = nanoOsMessageDataPointer(inputMessage, const char*);
////       }
////       break;
//// 
////     default:
////       // Do nothing.
////       break;
//// 
////   }
//// 
////   // It's possible we were passed a bad type that didn't result in the value of
////   // message being set, so only attempt to print it if it was set.
////   if (message != NULL) {
////     consolePrintMessage(consoleState, inputMessage, message);
////   }
//// 
////   processMessageSetDone(inputMessage);
////   consoleMessageCleanup(inputMessage);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleGetBufferCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Command handler for the CONSOLE_GET_BUFFER command.  Gets a free
//// /// buffer from the provided ConsoleState and replies to the message sender with
//// /// a pointer to the buffer structure.
//// ///
//// /// @param consoleState A pointer to the ConsoleState structure held by the
//// ///   runConsole process.  The buffers in this state will be searched for
//// ///   something available to return to the message sender.
//// /// @param inputMessage A pointer to the ProcessMessage that was received from
//// ///   the process that sent the command.
//// ///
//// /// @return This function returns no value but does set the inputMessage to
//// /// done so that the calling process knows that we've handled the message.
//// /// Since this is a synchronous call, it also pushes a message onto the message
//// /// sender's queue with the free buffer on success.  On failure, the
//// /// inputMessage is marked as done but no response is sent.
//// void consoleGetBufferCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   // We're going to reuse the input message as the return message.
////   ProcessMessage *returnMessage = inputMessage;
////   NanoOsMessage *nanoOsMessage
////     = (NanoOsMessage*) processMessageData(returnMessage);
////   nanoOsMessage->func = 0;
////   nanoOsMessage->data = (intptr_t) NULL;
////   ProcessId callingPid = processId(processMessageFrom(inputMessage));
//// 
////   ConsoleBuffer *returnValue
////     = getAvailableConsoleBuffer(consoleState, callingPid);
////   if (returnValue != NULL) {
////     // Send the buffer back to the caller via the message we allocated earlier.
////     nanoOsMessage->data = (intptr_t) returnValue;
////     processMessageInit(returnMessage, CONSOLE_RETURNING_BUFFER,
////       nanoOsMessage, sizeof(*nanoOsMessage), true);
////     if (processMessageQueuePush(processMessageFrom(inputMessage), returnMessage)
////       != processSuccess
////     ) {
////       returnValue->inUse = false;
////     }
////   }
//// 
////   // Whether we were able to grab a buffer or not, we're now done with this
////   // call, so mark the input message handled.  This is a synchronous call and
////   // the caller is waiting on our response, so *DO NOT* release it.  The caller
////   // is responsible for releasing it when they've received the response.
////   processMessageSetDone(inputMessage);
////   return;
//// }
//// 
//// /// @fn void consoleWriteBufferCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Command handler for the CONSOLE_WRITE_BUFFER command.  Writes the
//// /// contents of the sent buffer to the console and then clears its inUse flag.
//// ///
//// /// @param consoleState A pointer to the ConsoleState structure held by the
//// ///   runConsole process.  Unused by this function.
//// /// @param inputMessage A pointer to the ProcessMessage that was received from
//// ///   the process that sent the command.
//// ///
//// /// @return This function returns no value but does set the inputMessage to
//// /// done so that the calling process knows that we've handled the message.
//// void consoleWriteBufferCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   (void) consoleState;
//// 
////   ConsoleBuffer *consoleBuffer
////     = nanoOsMessageDataPointer(inputMessage, ConsoleBuffer*);
////   if (consoleBuffer != NULL) {
////     const char *message = consoleBuffer->buffer;
////     if (message != NULL) {
////       consolePrintMessage(consoleState, inputMessage, message);
////     }
////   }
////   processMessageSetDone(inputMessage);
////   consoleMessageCleanup(inputMessage);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleSetPortShellCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Set the designated shell process ID for a port.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.  This contains a NanoOsMessage that contains a
//// ///   ConsolePortPidAssociation that will associate the port with the process
//// ///   if this function succeeds.
//// ///
//// /// @return This function returns no value, but it marks the inputMessage as
//// /// being 'done' on success and does *NOT* mark it on failure.
//// void consoleSetPortShellCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   ConsolePortPidUnion consolePortPidUnion;
////   consolePortPidUnion.nanoOsMessageData
////     = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
////   ConsolePortPidAssociation *consolePortPidAssociation
////     = &consolePortPidUnion.consolePortPidAssociation;
//// 
////   uint8_t consolePort = consolePortPidAssociation->consolePort;
////   ProcessId processId = consolePortPidAssociation->processId;
//// 
////   if (consolePort < CONSOLE_NUM_PORTS) {
////     consoleState->consolePorts[consolePort].shell = processId;
////     processMessageSetDone(inputMessage);
////     consoleMessageCleanup(inputMessage);
////   } else {
////     printString("ERROR:  Request to assign ownership of non-existent port ");
////     printInt(consolePort);
////     printString("\n");
////     // *DON'T* call processMessageRelease or processMessageSetDone here.  The lack of the
////     // message being done will indicate to the caller that there was a problem
////     // servicing the command.
////   }
//// 
////   return;
//// }
//// 
//// /// @fn void consoleAssignPortInputCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Assign a console port's input and possibly output to a running
//// /// process.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.  This contains a NanoOsMessage that contains a
//// ///   ConsolePortPidAssociation that will associate the port's input with the
//// ///   process if this function succeeds.
//// /// @param assignOutput Whether or not to assign the output as well as the
//// ///   input.
//// ///
//// /// @return This function returns no value, but it marks the inputMessage as
//// /// being 'done' on success and does *NOT* mark it on failure.
//// void consoleAssignPortHelper(
////   ConsoleState *consoleState, ProcessMessage *inputMessage, bool assignOutput
//// ) {
////   ConsolePortPidUnion consolePortPidUnion;
////   consolePortPidUnion.nanoOsMessageData
////     = nanoOsMessageDataValue(inputMessage, NanoOsMessageData);
////   ConsolePortPidAssociation *consolePortPidAssociation
////     = &consolePortPidUnion.consolePortPidAssociation;
//// 
////   uint8_t consolePort = consolePortPidAssociation->consolePort;
////   ProcessId processId = consolePortPidAssociation->processId;
//// 
////   if (consolePort < CONSOLE_NUM_PORTS) {
////     if (assignOutput == true) {
////       consoleState->consolePorts[consolePort].outputOwner = processId;
////     }
////     consoleState->consolePorts[consolePort].inputOwner = processId;
////     processMessageSetDone(inputMessage);
////     consoleMessageCleanup(inputMessage);
////   } else {
////     printString("ERROR:  Request to assign ownership of non-existent port ");
////     printInt(consolePort);
////     printString("\n");
////     // *DON'T* call processMessageRelease or processMessageSetDone here.  The
////     // lack of the message being done will indicate to the caller that there
////     // was a problem servicing the command.
////   }
//// 
////   return;
//// }
//// 
//// /// @fn void consoleAssignPortCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Assign a console port's input and output to a running process.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.  This contains a NanoOsMessage that contains a
//// ///   ConsolePortPidAssociation that will associate the port's input and output
//// ///   with the process if this function succeeds.
//// ///
//// /// @return This function returns no value, but it marks the inputMessage as
//// /// being 'done' on success and does *NOT* mark it on failure.
//// void consoleAssignPortCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   consoleAssignPortHelper(consoleState, inputMessage, true);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleAssignPortInputCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Assign a console port's input to a running process.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.  This contains a NanoOsMessage that contains a
//// ///   ConsolePortPidAssociation that will associate the port's input with the
//// ///   process if this function succeeds.
//// ///
//// /// @return This function returns no value, but it marks the inputMessage as
//// /// being 'done' on success and does *NOT* mark it on failure.
//// void consoleAssignPortInputCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   consoleAssignPortHelper(consoleState, inputMessage, false);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleReleasePortCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Release all the ports currently owned by a process.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.
//// ///
//// /// @return This function returns no value.
//// void consoleReleasePortCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   ProcessId owner = processId(processMessageFrom(inputMessage));
////   ConsolePort *consolePorts = consoleState->consolePorts;
//// 
////   for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     if (consolePorts[ii].outputOwner == owner) {
////       consolePorts[ii].outputOwner = consolePorts[ii].shell;
////     }
////     if (consolePorts[ii].inputOwner == owner) {
////       consolePorts[ii].inputOwner = consolePorts[ii].shell;
////     }
////   }
//// 
////   // Since piped commands still attempt to release a port on completion, we
////   // will not print a warning if we didn't successfully release anything.
//// 
////   processMessageSetDone(inputMessage);
////   consoleMessageCleanup(inputMessage);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleGetOwnedPortCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Get the first port currently owned by a process.
//// ///
//// /// @note While it is technically possible for a single process to own multiple
//// /// ports, the expectation here is that this call is made by a process that is
//// /// only expecting to own one.  This is mostly for the purposes of transferring
//// /// ownership of the port from one process to another.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.
//// ///
//// /// @return This function returns no value.
//// void consoleGetOwnedPortCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   ProcessId owner = processId(processMessageFrom(inputMessage));
////   ConsolePort *consolePorts = consoleState->consolePorts;
////   ProcessMessage *returnMessage = inputMessage;
//// 
////   int ownedPort = -1;
////   for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     // inputOwner is assigned at the same as outputOwner, but inputOwner can be
////     // set separately later if the commands are being piped together.
////     // Therefore, checking inputOwner checks both of them.
////     if (consolePorts[ii].inputOwner == owner) {
////       ownedPort = ii;
////       break;
////     }
////   }
//// 
////   NanoOsMessage *nanoOsMessage
////     = (NanoOsMessage*) processMessageData(returnMessage);
////   nanoOsMessage->func = 0;
////   nanoOsMessage->data = (intptr_t) ownedPort;
////   processMessageInit(returnMessage, CONSOLE_RETURNING_PORT,
////     nanoOsMessage, sizeof(*nanoOsMessage), true);
////   sendProcessMessageToPid(owner, inputMessage);
//// 
////   processMessageSetDone(inputMessage);
////   consoleMessageCleanup(inputMessage);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleSetEchoCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Set whether or not input is echoed back to all console ports owned
//// /// by a process.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.
//// ///
//// /// @return This function returns no value.
//// void consoleSetEchoCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   ProcessId owner = processId(processMessageFrom(inputMessage));
////   ConsolePort *consolePorts = consoleState->consolePorts;
////   ProcessMessage *returnMessage = inputMessage;
////   bool desiredEchoState = nanoOsMessageDataValue(inputMessage, bool);
////   NanoOsMessage *nanoOsMessage
////     = (NanoOsMessage*) processMessageData(returnMessage);
////   nanoOsMessage->func = 0;
////   nanoOsMessage->data = 0;
//// 
////   bool portFound = false;
////   for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     if (consolePorts[ii].outputOwner == owner) {
////       consolePorts[ii].echo = desiredEchoState;
////       portFound = true;
////     }
////   }
//// 
////   if (portFound == false) {
////     printString("WARNING:  Request to set echo from non-owning process ");
////     printInt(owner);
////     printString("\n");
////     nanoOsMessage->data = (intptr_t) -1;
////   }
//// 
////   processMessageInit(returnMessage, CONSOLE_RETURNING_PORT,
////     nanoOsMessage, sizeof(*nanoOsMessage), true);
////   sendProcessMessageToPid(owner, inputMessage);
////   processMessageSetDone(inputMessage);
////   consoleMessageCleanup(inputMessage);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleWaitForInputCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Wait for input from any of the console ports owned by a process.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.
//// ///
//// /// @return This function returns no value.
//// void consoleWaitForInputCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   ProcessId owner = processId(processMessageFrom(inputMessage));
////   ConsolePort *consolePorts = consoleState->consolePorts;
//// 
////   bool portFound = false;
////   for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     if (consolePorts[ii].inputOwner == owner) {
////       consolePorts[ii].waitingForInput = true;
////       portFound = true;
////     }
////   }
//// 
////   if (portFound == false) {
////     printString("WARNING:  Request to wait for input from non-owning process ");
////     printInt(owner);
////     printString("\n");
////   }
//// 
////   processMessageSetDone(inputMessage);
////   consoleMessageCleanup(inputMessage);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleReleasePidPortCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Release all the ports currently owned by a process.
//// ///
//// /// @param consoleState A pointer to the ConsoleState being maintained by the
//// ///   runConsole function that's running.
//// /// @param inputMessage A pointer to the ProcessMessage with the received
//// ///   command.
//// ///
//// /// @return This function returns no value.
//// void consoleReleasePidPortCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   ProcessId sender = processId(processMessageFrom(inputMessage));
////   if (sender != NANO_OS_SCHEDULER_PROCESS_ID) {
////     // Sender is not the scheduler.  We will ignore this.
////     processMessageSetDone(inputMessage);
////     consoleMessageCleanup(inputMessage);
////     return;
////   }
//// 
////   ProcessId owner
////     = nanoOsMessageDataValue(inputMessage, ProcessId);
////   ConsolePort *consolePorts = consoleState->consolePorts;
////   ProcessMessage *processMessage
////     = nanoOsMessageFuncPointer(inputMessage, ProcessMessage*);
////   bool releaseMessage = false;
//// 
////   bool portFound = false;
////   for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     if (consolePorts[ii].inputOwner == owner) {
////       consolePorts[ii].inputOwner = consolePorts[ii].shell;
////       // NOTE:  By calling sendProcessMessageToPid from within the for loop, we
////       // run the risk of sending the same message to multiple shells.  That's
////       // irrelevant in this case since nothing is waiting for the message and
////       // all the shells will release the message.  In reality, one process
////       // almost never owns multiple ports.  The only exception is during boot.
////       if (owner != consolePorts[ii].shell) {
////         sendProcessMessageToPid(consolePorts[ii].shell, processMessage);
////       } else {
////         // The shell is being restarted.  It won't be able to receive the
////         // message if we send it, so we need to go ahead and release it.
////         releaseMessage = true;
////       }
////       portFound = true;
////     }
////     if (consolePorts[ii].outputOwner == owner) {
////       consolePorts[ii].outputOwner = consolePorts[ii].shell;
////       if (owner == consolePorts[ii].shell) {
////         // The shell is being restarted.  It won't be able to receive the
////         // message if we send it, so we need to go ahead and release it.
////         releaseMessage = true;
////       }
////       portFound = true;
////     }
////   }
//// 
////   if ((releaseMessage == true) || (portFound == false)) {
////     processMessageRelease(processMessage);
////   }
//// 
////   processMessageSetDone(inputMessage);
////   consoleMessageCleanup(inputMessage);
//// 
////   return;
//// }
//// 
//// /// @fn void consoleReleaseBufferCommandHandler(
//// ///   ConsoleState *consoleState, ProcessMessage *inputMessage)
//// ///
//// /// @brief Command handler for the CONSOLE_RELEASE_BUFFER command.  Releases a
//// /// buffer that was previously returned by the CONSOLE_GET_BUFFER command.
//// /// a pointer to the buffer structure.
//// ///
//// /// @param consoleState A pointer to the ConsoleState structure held by the
//// ///   runConsole process.  One of the buffers in this state will have its inUse
//// ///   member set to false.
//// /// @param inputMessage A pointer to the ProcessMessage that was received from
//// ///   the process that sent the command.
//// ///
//// /// @return This function returns no value but does set the inputMessage to
//// /// done so that the calling process knows that we've handled the message.
//// /// Since this is a synchronous call, it also pushes a message onto the message
//// /// sender's queue with the free buffer on success.  On failure, the
//// /// inputMessage is marked as done but no response is sent.
//// void consoleReleaseBufferCommandHandler(
////   ConsoleState *consoleState, ProcessMessage *inputMessage
//// ) {
////   // We don't need to examine consoleState since the caller has provided us
////   // with the pointer we can use directly.
////   (void) consoleState;
//// 
////   ConsoleBuffer *consoleBuffers = consoleState->consoleBuffers;
////   ConsoleBuffer *consoleBuffer
////     = nanoOsMessageDataPointer(inputMessage, ConsoleBuffer*);
////   if (consoleBuffer != NULL) {
////     for (int ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////       if (consoleBuffer == &consoleBuffers[ii]) {
////         // The buffer being released is one of the buffers dedicated to a port.
////         // *DO NOT* mark it as not being in use because it is always in use.
////         // Just release the message and return.
////         processMessageRelease(inputMessage);
////         return;
////       }
////     }
//// 
////     free(consoleBuffer); consoleBuffer = NULL;
////   }
//// 
////   // Whether we were able to grab a buffer or not, we're now done with this
////   // call, so mark the input message handled.  This is a synchronous call and
////   // the caller is waiting on our response, so *DO NOT* release it.  The caller
////   // is responsible for releasing it when they've received the response.
////   processMessageRelease(inputMessage);
////   return;
//// }
//// 
//// /// @typedef ConsoleCommandHandler
//// ///
//// /// @brief Signature of command handler for a console command.
//// typedef void (*ConsoleCommandHandler)(ConsoleState*, ProcessMessage*);
//// 
//// /// @var consoleCommandHandlers
//// ///
//// /// @brief Array of handlers for console command messages.
//// const ConsoleCommandHandler consoleCommandHandlers[] = {
////   consoleWriteValueCommandHandler,      // CONSOLE_WRITE_VALUE
////   consoleGetBufferCommandHandler,       // CONSOLE_GET_BUFFER
////   consoleWriteBufferCommandHandler,     // CONSOLE_WRITE_BUFFER
////   consoleSetPortShellCommandHandler,    // CONSOLE_SET_PORT_SHELL
////   consoleAssignPortCommandHandler,      // CONSOLE_ASSIGN_PORT
////   consoleAssignPortInputCommandHandler, // CONSOLE_ASSIGN_PORT_INPUT
////   consoleReleasePortCommandHandler,     // CONSOLE_RELEASE_PORT
////   consoleGetOwnedPortCommandHandler,    // CONSOLE_GET_OWNED_PORT
////   consoleSetEchoCommandHandler,         // CONSOLE_SET_ECHO_PORT
////   consoleWaitForInputCommandHandler,    // CONSOLE_WAIT_FOR_INPUT
////   consoleReleasePidPortCommandHandler,  // CONSOLE_RELEASE_PID_PORT
////   consoleReleaseBufferCommandHandler,   // CONSOLE_RELEASE_BUFFER
//// };
//// 
//// /// @fn void handleConsoleMessages(ConsoleState *consoleState)
//// ///
//// /// @brief Handle all messages currently in the console process's message queue.
//// ///
//// /// @param consoleState A pointer to the ConsoleState structure maintained by
//// ///   the runConsole process.
//// ///
//// /// @return This function returns no value.
//// void handleConsoleMessages(ConsoleState *consoleState) {
////   ProcessMessage *message = processMessageQueuePop();
////   while (message != NULL) {
////     ConsoleCommand messageType = (ConsoleCommand) processMessageType(message);
////     if (messageType >= NUM_CONSOLE_COMMANDS) {
////       // Invalid.
////       message = processMessageQueuePop();
////       continue;
////     }
//// 
////     consoleCommandHandlers[messageType](consoleState, message);
////     message = processMessageQueuePop();
////   }
//// 
////   return;
//// }
//// 
//// /// @fn int readSerialByte(ConsolePort *consolePort, UartClass &serialPort)
//// ///
//// /// @brief Do a non-blocking read of a serial port.
//// ///
//// /// @param ConsolePort A pointer to the ConsolePort data structure that contains
//// ///   the buffer information to use.
//// /// @param serialPort A reference to the UartClass object (Serial or Serial1)
//// ///   to read a byte from.
//// ///
//// /// @return Returns the byte read, cast to an int, on success, -1 on failure.
//// int readSerialByte(ConsolePort *consolePort, UartClass &serialPort) {
////   int serialData = -1;
////   serialData = serialPort.read();
////   if (serialData > -1) {
////     ConsoleBuffer *consoleBuffer = consolePort->consoleBuffer;
////     char *buffer = consoleBuffer->buffer;
////     buffer[consolePort->consoleIndex] = (char) serialData;
////     if (consolePort->echo == true) {
////       if (((char) serialData != '\r')
////         && ((char) serialData != '\n')
////       ) {
////         serialPort.print((char) serialData);
////       } else {
////         serialPort.print("\r\n");
////       }
////     }
////     consolePort->consoleIndex++;
////     consolePort->consoleIndex %= CONSOLE_BUFFER_SIZE;
////   }
//// 
////   return serialData;
//// }
//// 
//// /// @fn int readUsbSerialByte(ConsolePort *consolePort)
//// ///
//// /// @brief Do a non-blocking read of the USB serial port.
//// ///
//// /// @param ConsolePort A pointer to the ConsolePort data structure that contains
//// ///   the buffer information to use.
//// ///
//// /// @return Returns the byte read, cast to an int, on success, -1 on failure.
//// int readUsbSerialByte(ConsolePort *consolePort) {
////   return readSerialByte(consolePort, Serial);
//// }
//// 
//// /// @fn int readGpioSerialByte(ConsolePort *consolePort)
//// ///
//// /// @brief Do a non-blocking read of the GPIO serial port.
//// ///
//// /// @param ConsolePort A pointer to the ConsolePort data structure that contains
//// ///   the buffer information to use.
//// ///
//// /// @return Returns the byte read, cast to an int, on success, -1 on failure.
//// int readGpioSerialByte(ConsolePort *consolePort) {
////   return readSerialByte(consolePort, Serial1);
//// }
//// 
//// /// @fn int printSerialString(UartClass &serialPort, const char *string)
//// ///
//// /// @brief Print a string to the default serial port.
//// ///
//// /// @param serialPort A reference to the UartClass object (Serial or Serial1)
//// ///   to read a byte from.
//// /// @param string A pointer to the string to print.
//// ///
//// /// @return Returns the number of bytes written to the serial port.
//// int printSerialString(UartClass &serialPort, const char *string) {
////   int returnValue = 0;
////   size_t numBytes = 0;
//// 
////   char *newlineAt = strchr(string, '\n');
////   newlineAt = strchr(string, '\n');
////   if (newlineAt == NULL) {
////     numBytes = strlen(string);
////   } else {
////     numBytes = (size_t) (((uintptr_t) newlineAt) - ((uintptr_t) string));
////   }
////   while (newlineAt != NULL) {
////     returnValue += (int) serialPort.write(string, numBytes);
////     returnValue += (int) serialPort.write("\r\n");
////     string = newlineAt + 1;
////     newlineAt = strchr(string, '\n');
////     if (newlineAt == NULL) {
////       numBytes = strlen(string);
////     } else {
////       numBytes = (size_t) (((uintptr_t) newlineAt) - ((uintptr_t) string));
////     }
////   }
////   returnValue += (int) serialPort.write(string, numBytes);
//// 
////   return returnValue;
//// }
//// 
//// /// @fn int printUsbSerialString(const char *string)
//// ///
//// /// @brief Print a string to the USB serial port.
//// ///
//// /// @param string A pointer to the string to print.
//// ///
//// /// @return Returns the number of bytes written to the serial port.
//// int printUsbSerialString(const char *string) {
////   return printSerialString(Serial, string);
//// }
//// 
//// /// @fn int printGpioSerialString(const char *string)
//// ///
//// /// @brief Print a string to the GPIO serial port.
//// ///
//// /// @param string A pointer to the string to print.
//// ///
//// /// @return Returns the number of bytes written to the serial port.
//// int printGpioSerialString(const char *string) {
////   return printSerialString(Serial1, string);
//// }
//// 
//// /// @fn void* runConsole(void *args)
//// ///
//// /// @brief Main process for managing console input and output.  Runs in an
//// /// infinite loop and never exits.  Every iteration, it checks the serial
//// /// connection for a byte and adds it to the buffer if there is anything,
//// /// handles the user command if the incoming byte is a newline, and handles any
//// /// messages that were sent to this process.
//// ///
//// /// @param args Any arguments provided by the scheduler.  Ignored by this
//// ///   process.
//// ///
//// /// @return This function never returns, but would return NULL if it did.
//// void* runConsole(void *args) {
////   (void) args;
//// 
////   int byteRead = -1;
////   ConsoleState consoleState;
////   memset(&consoleState, 0, sizeof(ConsoleState));
////   ProcessMessage *schedulerMessage = NULL;
////   printDebug("sizeof(ConsoleState) = ");
////   printDebug(sizeof(ConsoleState));
////   printDebug("\n");
//// 
////   for (uint8_t ii = 0; ii < CONSOLE_NUM_BUFFERS; ii++) {
////     consoleState.consoleBuffers[ii].inUse = false;
////   }
//// 
////   // For each console port, use the console buffer at the corresponding index.
////   for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////     consoleState.consolePorts[ii].consoleBuffer
////       = &consoleState.consoleBuffers[ii];
////     consoleState.consolePorts[ii].consoleBuffer->inUse = true;
////   }
//// 
////   // Set the port-specific data.
////   consoleState.consolePorts[USB_SERIAL_PORT].consoleIndex = 0;
////   consoleState.consolePorts[USB_SERIAL_PORT].inputOwner = PROCESS_ID_NOT_SET;
////   consoleState.consolePorts[USB_SERIAL_PORT].outputOwner = PROCESS_ID_NOT_SET;
////   consoleState.consolePorts[USB_SERIAL_PORT].shell = PROCESS_ID_NOT_SET;
////   consoleState.consolePorts[USB_SERIAL_PORT].waitingForInput = false;
////   consoleState.consolePorts[USB_SERIAL_PORT].readByte = readUsbSerialByte;
////   consoleState.consolePorts[USB_SERIAL_PORT].echo = true;
////   consoleState.consolePorts[USB_SERIAL_PORT].printString
////     = printUsbSerialString;
//// 
////   consoleState.consolePorts[GPIO_SERIAL_PORT].consoleIndex = 0;
////   consoleState.consolePorts[GPIO_SERIAL_PORT].inputOwner = PROCESS_ID_NOT_SET;
////   consoleState.consolePorts[GPIO_SERIAL_PORT].outputOwner = PROCESS_ID_NOT_SET;
////   consoleState.consolePorts[GPIO_SERIAL_PORT].shell = PROCESS_ID_NOT_SET;
////   consoleState.consolePorts[GPIO_SERIAL_PORT].waitingForInput = false;
////   consoleState.consolePorts[GPIO_SERIAL_PORT].readByte = readGpioSerialByte;
////   consoleState.consolePorts[GPIO_SERIAL_PORT].echo = true;
////   consoleState.consolePorts[GPIO_SERIAL_PORT].printString
////     = printGpioSerialString;
//// 
////   while (1) {
////     for (uint8_t ii = 0; ii < CONSOLE_NUM_PORTS; ii++) {
////       ConsolePort *consolePort = &consoleState.consolePorts[ii];
////       byteRead = consolePort->readByte(consolePort);
////       if ((byteRead == ((int) '\n')) || (byteRead == ((int) '\r'))) {
////         if (consolePort->inputOwner == PROCESS_ID_NOT_SET) {
////           // NULL-terminate the buffer.
////           consolePort->consoleIndex--;
////           consolePort->consoleBuffer->buffer[consolePort->consoleIndex] = '\0';
////           if (byteRead == ((int) '\r')) {
////             consolePort->printString("\n");
////           }
//// 
////           // Use consoleIndex as the size to create a buffer and make a copy.
////           char *bufferCopy = (char*) malloc(consolePort->consoleIndex + 1);
////           memcpy(bufferCopy, consolePort->consoleBuffer->buffer,
////             consolePort->consoleIndex);
////           bufferCopy[consolePort->consoleIndex] = '\0';
////           consolePort->consoleIndex = 0;
////           //// if (handleCommand(ii, bufferCopy) == processSuccess) {
////           ////   // If the command has already returned or wrote to the console
////           ////   // before its first yield, we may need to display its output.
////           ////   // Handle the next next message in our queue just in case.
////           ////   handleConsoleMessages(&consoleState);
////           //// } else {
////           ////   consolePort->printString("Unknown command.\n");
////           ////   consolePort->printString("> ");
////           //// }
////         } else if (consolePort->waitingForInput == true) {
////           consolePort->consoleBuffer->buffer[consolePort->consoleIndex] = '\0';
////           consolePort->consoleIndex = 0;
////           sendNanoOsMessageToPid(
////             consolePort->inputOwner, CONSOLE_RETURNING_INPUT,
////             /* func= */ 0, (intptr_t) consolePort->consoleBuffer, false);
////           consolePort->waitingForInput = false;
////         } else {
////           // Console port is owned but owning process is not waiting for input.
////           // Reset our buffer and do nothing.
////           consolePort->consoleIndex = 0;
////         }
////       }
////     }
//// 
////     schedulerMessage = (ProcessMessage*) coroutineYield(NULL);
//// 
////     if (schedulerMessage != NULL) {
////       // We have a message from the scheduler that we need to process.  This
////       // is not the expected case, but it's the priority case, so we need to
////       // list it first.
////       ConsoleCommand messageType
////         = (ConsoleCommand) processMessageType(schedulerMessage);
////       if (messageType < NUM_CONSOLE_COMMANDS) {
////         consoleCommandHandlers[messageType](&consoleState, schedulerMessage);
////       } else {
////         printString("ERROR!!!  Received unknown console command ");
////         printInt(messageType);
////         printString(" from scheduler.\n");
////       }
////     } else {
////       // No message from the scheduler.  Handle any user process messages in
////       // our message queue.
////       handleConsoleMessages(&consoleState);
////     }
////   }
//// 
////   return NULL;
//// }
//// 
//// /// @fn int printConsoleValue(
//// ///   ConsoleValueType valueType, void *value, size_t length)
//// ///
//// /// @brief Send a command to print a value to the console.
//// ///
//// /// @param command The ConsoleCommand of the message to send, which will
//// ///   determine the handler to use and therefore the type of the value that is
//// ///   displayed.
//// /// @param value A pointer to the value to send as data.
//// /// @param length The length of the data at the address provided by the value
//// ///   pointer.  Will be truncated to the size of NanoOsMessageData if needed.
//// ///
//// /// @return This function is non-blocking, always succeeds, and always returns
//// /// 0.
//// int printConsoleValue(ConsoleValueType valueType, void *value, size_t length) {
////   NanoOsMessageData message = 0;
////   length = (length <= sizeof(message)) ? length : sizeof(message);
////   memcpy(&message, value, length);
//// 
////   sendNanoOsMessageToPid(NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_WRITE_VALUE,
////     valueType, message, false);
//// 
////   return 0;
//// }
//// 
//// /// @fn printConsole(<type> message)
//// ///
//// /// @brief Print a message of an arbitrary type to the console.
//// ///
//// /// @details
//// /// This is basically just a switch statement where the type is the switch
//// /// value.  They all call printConsole with the appropriate console command, a
//// /// pointer to the provided message, and the size of the message.
//// ///
//// /// @param message The message to send to the console.
//// ///
//// /// @return Returns the value returned by printConsoleValue.
//// int printConsole(char message) {
////   return printConsoleValue(CONSOLE_VALUE_CHAR, &message, sizeof(message));
//// }
//// int printConsole(unsigned char message) {
////   return printConsoleValue(CONSOLE_VALUE_UCHAR, &message, sizeof(message));
//// }
//// int printConsole(int message) {
////   return printConsoleValue(CONSOLE_VALUE_INT, &message, sizeof(message));
//// }
//// int printConsole(unsigned int message) {
////   return printConsoleValue(CONSOLE_VALUE_UINT, &message, sizeof(message));
//// }
//// int printConsole(long int message) {
////   return printConsoleValue(CONSOLE_VALUE_LONG_INT, &message, sizeof(message));
//// }
//// int printConsole(long unsigned int message) {
////   return printConsoleValue(CONSOLE_VALUE_LONG_UINT, &message, sizeof(message));
//// }
//// int printConsole(float message) {
////   return printConsoleValue(CONSOLE_VALUE_FLOAT, &message, sizeof(message));
//// }
//// int printConsole(double message) {
////   return printConsoleValue(CONSOLE_VALUE_DOUBLE, &message, sizeof(message));
//// }
//// int printConsole(const char *message) {
////   return printConsoleValue(CONSOLE_VALUE_STRING, &message, sizeof(message));
//// }
//// 
//// // Console port support functions.
//// 
//// /// @fn void releaseConsole(void)
//// ///
//// /// @brief Release the console and display the command prompt to the user again.
//// ///
//// /// @return This function returns no value.
//// void releaseConsole(void) {
////   // releaseConsole is sometimes called from within handleCommand, which runs
////   // from within the console process.  That means we can't do blocking prints
////   // from this function.  i.e. We can't use printf here.  Use printConsole
////   // instead.
////   sendNanoOsMessageToPid(NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_RELEASE_PORT,
////     /* func= */ 0, /* data= */ 0, false);
////   processYield();
//// }
//// 
//// /// @fn int getOwnedConsolePort(void)
//// ///
//// /// @brief Get the first port owned by the running process.
//// ///
//// /// @return Returns the numerical index of the console port the process owns on
//// /// success, -1 on failure.
//// int getOwnedConsolePort(void) {
////   ProcessMessage *sent = sendNanoOsMessageToPid(
////     NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_GET_OWNED_PORT,
////     /* func= */ 0, /* data= */ 0, /* waiting= */ true);
//// 
////   // The console will reuse the message we sent, so don't release the message
////   // in processMessageWaitForReplyWithType.
////   ProcessMessage *reply = processMessageWaitForReplyWithType(
////     sent, /* releaseAfterDone= */ false,
////     CONSOLE_RETURNING_PORT, NULL);
//// 
////   int returnValue = nanoOsMessageDataValue(reply, int);
////   processMessageRelease(reply);
//// 
////   return returnValue;
//// }
//// 
//// /// @fn int setConsoleEcho(bool desiredEchoState)
//// ///
//// /// @brief Get the echo state for all ports owned by the current process.
//// ///
//// /// @return Returns 0 if the echo state was set for the current process's
//// /// ports, -1 on failure.
//// int setConsoleEcho(bool desiredEchoState) {
////   ProcessMessage *sent = sendNanoOsMessageToPid(
////     NANO_OS_CONSOLE_PROCESS_ID, CONSOLE_SET_ECHO_PORT,
////     /* func= */ 0, /* data= */ desiredEchoState, /* waiting= */ true);
//// 
////   // The console will reuse the message we sent, so don't release the message
////   // in processMessageWaitForReplyWithType.
////   ProcessMessage *reply = processMessageWaitForReplyWithType(
////     sent, /* releaseAfterDone= */ false,
////     CONSOLE_RETURNING_PORT, NULL);
//// 
////   int returnValue = nanoOsMessageDataValue(reply, int);
////   processMessageRelease(reply);
//// 
////   return returnValue;
//// }

