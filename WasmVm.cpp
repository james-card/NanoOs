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

#include "WasmVm.h"

/// @fn int32_t wasmStackPush32(VirtualMemoryState *stack, uint32_t value)
///
/// @brief Push a 32-bit value onto a WASM stack
///
/// @param stack Pointer to virtual memory representing the stack
/// @param value The 32-bit value to push
///
/// @return Returns 0 on success, -1 on error
int32_t wasmStackPush32(VirtualMemoryState *stack, uint32_t value) {
  // Get current stack pointer (top of stack)
  uint32_t stackPointer;
  if (virtualMemoryRead32(stack, 0, &stackPointer) != 0) {
    return -1;
  }

  // Check for stack overflow
  if (stackPointer >= stack->fileSize - sizeof(uint32_t)) {
    return -1;
  }

  // Write value to stack
  if (virtualMemoryWrite32(
    stack, stackPointer + sizeof(stackPointer), value) != 0
  ) {
    return -1;
  }

  // Update stack pointer
  stackPointer += sizeof(uint32_t);
  if (virtualMemoryWrite32(stack, 0, stackPointer) != 0) {
    return -1;
  }

  return 0;
}

/// @fn int32_t wasmStackPop32(VirtualMemoryState *stack, uint32_t *value)
///
/// @brief Pop a 32-bit value from a WASM stack
///
/// @param stack Pointer to virtual memory representing the stack
/// @param value Pointer where to store the popped value
///
/// @return Returns 0 on success, -1 on error
int32_t wasmStackPop32(VirtualMemoryState *stack, uint32_t *value) {
  // Get current stack pointer
  uint32_t stackPointer;
  if (virtualMemoryRead32(stack, 0, &stackPointer) != 0) {
    return -1;
  }

  // Check for stack underflow
  if (stackPointer < sizeof(uint32_t)) {
    return -1;
  }

  // Update stack pointer first
  stackPointer -= sizeof(uint32_t);
  if (virtualMemoryWrite32(stack, 0, stackPointer) != 0) {
    return -1;
  }

  // Read value from stack
  if (virtualMemoryRead32(
    stack, stackPointer + sizeof(stackPointer), value) != 0
  ) {
    return -1;
  }

  return 0;
}

/// @fn int32_t wasmStackInit(VirtualMemoryState *stack)
///
/// @brief Initialize a WASM stack
///
/// @param stack Pointer to virtual memory representing the stack
///
/// @return Returns 0 on success, -1 on error
int32_t wasmStackInit(VirtualMemoryState *stack) {
  // Initialize stack pointer to 0
  return virtualMemoryWrite32(stack, 0, 0);
}

/// @fn uint32_t readLeb128(VirtualMemoryState *memory, uint32_t offset,
///   uint32_t *value)
///
/// @brief Read a LEB128 encoded integer from memory
///
/// @param memory Virtual memory to read from
/// @param offset Offset to read from
/// @param value Pointer to store decoded value
///
/// @return Number of bytes read, or 0 on error
uint32_t readLeb128(VirtualMemoryState *memory, uint32_t offset, 
  uint32_t *value
) {
  uint32_t result = 0;
  uint32_t shift = 0;
  uint32_t bytesRead = 0;
  uint8_t byte;

  do {
    if (virtualMemoryRead8(memory, offset + bytesRead, &byte) != 0) {
      return 0;
    }
    result |= ((byte & 0x7f) << shift);
    shift += 7;
    bytesRead++;
  } while (byte & 0x80);

  *value = result;
  return bytesRead;
}

/// @fn int32_t wasmFindImportFunction(const char *fullName, 
///   const WasmImport *importTable, uint32_t importCount, uint32_t *index)
///
/// @brief Find import function using binary search
///
/// @param fullName Full function name (module.field)
/// @param importTable Array of import functions to search
/// @param importCount Number of entries in importTable
/// @param index Where to store the found index
///
/// @return Returns 0 if found, -1 if not found
int32_t wasmFindImportFunction(const char *fullName, 
  const WasmImport *importTable, uint32_t importCount, uint32_t *index
) {
  int32_t left = 0;
  int32_t right = importCount - 1;
  
  while (left <= right) {
    int32_t mid = (left + right) / 2;
    int32_t comparison = strcmp(fullName, importTable[mid].functionName);
    
    if (comparison == 0) {
      *index = mid;
      return 0;
    } else if (comparison < 0) {
      right = mid - 1;
    } else {
      left = mid + 1;
    }
  }
  
  return -1;
}

/// @fn int32_t wasmFindSection(WasmVm *wasmVm, uint8_t sectionId,
///   uint32_t *sectionOffset, uint32_t *sectionSize)
///
/// @brief Find a specific section in a WASM module
///
/// @param wasmVm Pointer to WASM VM state
/// @param sectionId ID of the section to find
/// @param sectionOffset Where to store the offset of section content
/// @param sectionSize Where to store the size of section content
///
/// @return Returns 0 if found, -1 if not found or error
int32_t wasmFindSection(WasmVm *wasmVm, uint8_t sectionId,
  uint32_t *sectionOffset, uint32_t *sectionSize
) {
  uint32_t offset = 8; // Skip WASM header
  uint8_t currentSectionId;
  uint32_t currentSectionSize;
  uint32_t bytesRead;
  
  while (1) {
    if (virtualMemoryRead8(&wasmVm->codeSegment, offset,
      &currentSectionId) != 0
    ) {
      return -1;
    }
    offset++;
    
    bytesRead = readLeb128(&wasmVm->codeSegment, offset, &currentSectionSize);
    if (bytesRead == 0) {
      return -1;
    }
    offset += bytesRead;
    
    if (currentSectionId == sectionId) {
      *sectionOffset = offset;
      *sectionSize = currentSectionSize;
      return 0;
    }
    
    offset += currentSectionSize;
    
    // Check if we've reached the end
    uint8_t nextByte;
    if (virtualMemoryRead8(&wasmVm->codeSegment, offset, &nextByte) != 0) {
      break;
    }
  }
  
  return -1; // Section not found
}

/// @fn int32_t wasmParseImports(
///   WasmVm *wasmVm, const WasmImport *importTable, uint32_t importTableLength)
///
/// @brief Parse the imports section of a WASM module and populate table space
///
/// @param wasmVm Pointer to WASI VM state
/// @param importTable Array of import functions to use
/// @param importTableLength Number of entries in importTable
///
/// @return Returns 0 on success, -1 on error
int32_t wasmParseImports(
  WasmVm *wasmVm, const WasmImport *importTable, uint32_t importTableLength
) {
  uint32_t offset = 8;  // Skip WASM header
  uint32_t sectionSize;
  uint32_t importCount;
  char *importName = NULL;
  int32_t returnValue = -1;
  uint32_t tableIndex = 0;  // Current index in table space
  uint32_t bytesRead = 0;
  
  // Find imports section
  if (
    wasmFindSection(wasmVm, WASM_SECTION_IMPORTS, &offset, &sectionSize) != 0
  ) {
    return -1;
  }
  
  // Read number of imports
  bytesRead = readLeb128(&wasmVm->codeSegment, offset, &importCount);
  if (bytesRead == 0) {
    return -1;
  }
  offset += bytesRead;
  
  // Process each import
  for (uint32_t ii = 0; ii < importCount; ii++) {
    uint32_t moduleNameLength;
    
    // Read module name length
    bytesRead = readLeb128(&wasmVm->codeSegment, offset,
      &moduleNameLength);
    if (bytesRead == 0) {
      goto cleanup;
    }
    offset += bytesRead;
    
    // Allocate space for module name + '.' + null terminator
    importName = (char *) malloc(moduleNameLength + 2);
    if (importName == NULL) {
      goto cleanup;
    }
    
    // Read module name and add '.'
    if (virtualMemoryRead(&wasmVm->codeSegment, offset, moduleNameLength,
      importName) != moduleNameLength
    ) {
      goto cleanup;
    }
    importName[moduleNameLength] = '.';
    importName[moduleNameLength + 1] = '\0';
    offset += moduleNameLength;
    
    // Read field name length
    uint32_t fieldNameLength;
    bytesRead = readLeb128(&wasmVm->codeSegment, offset,
      &fieldNameLength);
    if (bytesRead == 0) {
      goto cleanup;
    }
    offset += bytesRead;
    
    // Reallocate to make room for field name
    char *newPtr = (char *) realloc(importName, 
      moduleNameLength + fieldNameLength + 2);
    if (newPtr == NULL) {
      goto cleanup;
    }
    importName = newPtr;
    
    // Read field name
    if (virtualMemoryRead(&wasmVm->codeSegment, offset, fieldNameLength,
      importName + moduleNameLength + 1) != fieldNameLength
    ) {
      goto cleanup;
    }
    importName[moduleNameLength + fieldNameLength + 1] = '\0';
    offset += fieldNameLength;
    
    // Read import kind
    uint8_t importKind;
    if (virtualMemoryRead8(&wasmVm->codeSegment, offset,
      &importKind) != 0
    ) {
      goto cleanup;
    }
    offset++;
    
    // For functions, read the type index and add to table
    if (importKind == 0) {
      uint32_t typeIndex;
      bytesRead = readLeb128(&wasmVm->codeSegment, offset, &typeIndex);
      if (bytesRead == 0) {
        goto cleanup;
      }
      offset += bytesRead;
      
      // Find the function in our table using binary search
      uint32_t functionIndex;
      if (wasmFindImportFunction(importName, importTable, importTableLength,
        &functionIndex) == 0
      ) {
        // Write function index to table space
        if (virtualMemoryWrite32(&wasmVm->tableSpace,
          tableIndex * sizeof(uint32_t), functionIndex) != 0
        ) {
          goto cleanup;
        }
        tableIndex++;
      }
    } else {
      // Skip other import kinds for now
      // TODO: Handle table, memory, and global imports
    }

    // Free current name before next iteration
    free(importName);
    importName = NULL;
  }
  
  // Store total end of table marker.
  if (virtualMemoryWrite32(&wasmVm->tableSpace, 
    tableIndex * sizeof(uint32_t), 0xFFFFFFFF) != 0
  ) {
    goto cleanup;
  }
  
  returnValue = 0;

cleanup:
  free(importName);
  return returnValue;
}

/// @fn int32_t wasmParseMemorySection(WasmVm *wasmVm)
///
/// @brief Parse the memory section of a WASM module to configure linear memory
///
/// @param wasmVm Pointer to WASM VM state
///
/// @return Returns 0 on success, -1 on error
int32_t wasmParseMemorySection(WasmVm *wasmVm) {
  uint32_t sectionOffset;
  uint32_t sectionSize;
  
  // Find memory section (section ID 5)
  if (wasmFindSection(wasmVm,
    WASM_SECTION_MEMORY, &sectionOffset, &sectionSize) != 0
  ) {
    return -1;
  }
  
  // Parse memory section content
  uint32_t count;
  uint32_t bytesRead = readLeb128(&wasmVm->codeSegment, sectionOffset, &count);
  if (bytesRead == 0) {
    return -1;
  }
  
  // Only handle one memory entry for now
  if (count != 1) {
    return -1;
  }
  
  // Read limits
  uint32_t offset = sectionOffset + bytesRead;
  uint8_t flags;
  if (virtualMemoryRead8(&wasmVm->codeSegment, offset, &flags) != 0) {
    return -1;
  }
  offset++;
  
  uint32_t initialPages;
  bytesRead = readLeb128(&wasmVm->codeSegment, offset, &initialPages);
  if (bytesRead == 0) {
    return -1;
  }
  
  // Allocate initial memory (64KB per page)
  uint32_t memorySize = initialPages * 65536;
  if (virtualMemoryWrite(&wasmVm->linearMemory, 0, memorySize, NULL) 
    != memorySize
  ) {
    return -1;
  }
  
  return 0;
}

/// @fn int32_t wasmFindStartFunction(WasmVm *wasmVm)
///
/// @brief Find the start function in the export section and set program counter
///
/// @param wasmVm Pointer to WASM VM state
///
/// @return Returns 0 on success, -1 on error
int32_t wasmFindStartFunction(WasmVm *wasmVm) {
  uint32_t sectionOffset;
  uint32_t sectionSize;
  
  // Find export section (section ID 7)
  if (
    wasmFindSection(wasmVm,
      WASM_SECTION_EXPORT, &sectionOffset, &sectionSize) != 0
  ) {
    return -1;
  }
  
  // Parse export section
  uint32_t exportCount;
  uint32_t bytesRead = readLeb128(
    &wasmVm->codeSegment, sectionOffset, &exportCount);
  if (bytesRead == 0) {
    return -1;
  }
  uint32_t offset = sectionOffset + bytesRead;
  
  // Look for "_start" export
  for (uint32_t ii = 0; ii < exportCount; ii++) {
    uint32_t nameLen;
    bytesRead = readLeb128(&wasmVm->codeSegment, offset, &nameLen);
    if (bytesRead == 0) {
      return -1;
    }
    offset += bytesRead;
    
    // Check if name matches "_start"
    char name[7];
    if (nameLen == 6) {
      if (virtualMemoryRead(&wasmVm->codeSegment, offset, 6, name) != 6) {
        return -1;
      }
      if (strncmp(name, "_start", 6) == 0) {
        // Found it - read kind and index
        offset += 6;
        uint8_t kind;
        if (virtualMemoryRead8(&wasmVm->codeSegment, offset, &kind) != 0) {
          return -1;
        }
        offset++;
        
        if (kind == 0) { // Function export
          uint32_t functionIndex;
          bytesRead = readLeb128(
            &wasmVm->codeSegment, offset, &functionIndex);
          if (bytesRead == 0) {
            return -1;
          }
          
          // Get function address from code section
          // Will need lookup in code section
          wasmVm->programCounter = functionIndex;
          return 0;
        }
      }
    }
    
    // Skip to next export
    offset += nameLen;
    uint8_t exportKind;
    if (virtualMemoryRead8(&wasmVm->codeSegment, offset, &exportKind) != 0) {
      return -1;
    }
    offset++;
    
    uint32_t exportIndex;
    bytesRead = readLeb128(&wasmVm->codeSegment, offset, &exportIndex);
    if (bytesRead == 0) {
      return -1;
    }
    offset += bytesRead;
  }
  
  return -1; // No start function found
}

/// @fn int32_t wasmInitCodeState(WasmVm *wasmVm)
///
/// @brief Find the code section and build the function table mapping
///
/// @param wasmVm Pointer to WASM VM state
///
/// @return Returns 0 on success, -1 on error
int32_t wasmInitCodeState(WasmVm *wasmVm) {
  uint32_t sectionOffset;
  uint32_t sectionSize;
  uint32_t currentOffset;
  uint32_t bytesRead;
  
  // Find code section (section ID 10)
  if (wasmFindSection(wasmVm,
    WASM_SECTION_CODE, &sectionOffset, &sectionSize) != 0
  ) {
    return -1;
  }
  
  // Read number of functions
  uint32_t functionCount;
  bytesRead = readLeb128(&wasmVm->codeSegment, sectionOffset, &functionCount);
  if (bytesRead == 0) {
    return -1;
  }
  currentOffset = sectionOffset + bytesRead;
  
  // Allocate function table
  wasmVm->codeState.functionTable = (WasmFunction *) malloc(
    sizeof(WasmFunction) * functionCount
  );
  if (wasmVm->codeState.functionTable == NULL) {
    return -1;
  }
  wasmVm->codeState.functionCount = functionCount;
  wasmVm->codeState.codeSectionOffset = sectionOffset;
  
  // Process each function
  for (uint32_t ii = 0; ii < functionCount; ii++) {
    // Read function body size
    uint32_t bodySize;
    bytesRead = readLeb128(&wasmVm->codeSegment, currentOffset, &bodySize);
    if (bytesRead == 0) {
      free(wasmVm->codeState.functionTable);
      wasmVm->codeState.functionTable = NULL;
      return -1;
    }
    
    // Store function location
    wasmVm->codeState.functionTable[ii].codeOffset = currentOffset + bytesRead;
    
    // Skip local declarations to find actual code start
    uint32_t localSetCount;
    uint32_t localOffset = currentOffset + bytesRead;
    bytesRead = readLeb128(&wasmVm->codeSegment, localOffset, &localSetCount);
    if (bytesRead == 0) {
      free(wasmVm->codeState.functionTable);
      wasmVm->codeState.functionTable = NULL;
      return -1;
    }
    localOffset += bytesRead;
    
    // Skip each local declaration set
    for (uint32_t jj = 0; jj < localSetCount; jj++) {
      uint32_t localCount;
      bytesRead = readLeb128(&wasmVm->codeSegment, localOffset, &localCount);
      if (bytesRead == 0) {
        free(wasmVm->codeState.functionTable);
        wasmVm->codeState.functionTable = NULL;
        return -1;
      }
      localOffset += bytesRead + 1; // +1 for value type byte
    }
    
    // Update offset to next function
    currentOffset += bytesRead + bodySize;
  }
  
  return 0;
}

/// @struct WasmStackFrame
///
/// @brief Structure to track function call stack frames
///
/// @param functionIndex Index of the currently executing function
/// @param returnAddress Address to return to after function completion
/// @param localCount Number of local variables in this frame
/// @param stackBase Base of operand stack for this frame
/// @param locals Flexible array member for local variables
typedef struct WasmStackFrame {
  uint32_t functionIndex;
  uint32_t returnAddress;
  uint32_t localCount;
  uint32_t stackBase;
  uint32_t locals[1];  // Flexible array member
} WasmStackFrame;

/// @fn uint32_t wasmGetFrameSize(uint32_t localCount)
///
/// @brief Calculate total size needed for a stack frame
///
/// @param localCount Number of local variables in the frame
///
/// @return Size in bytes needed for the frame
uint32_t wasmGetFrameSize(uint32_t localCount) {
  return sizeof(WasmStackFrame) 
    + ((localCount > 1) ? (localCount - 1) * sizeof(uint32_t) : 0);
}

/// @fn int32_t wasmPushFrame(WasmVm *wasmVm, uint32_t functionIndex)
///
/// @brief Push a new stack frame for a function call
///
/// @param wasmVm Pointer to the WASM VM state
/// @param functionIndex Index of the function being called
///
/// @return Returns 0 on success, -1 on error
int32_t wasmPushFrame(WasmVm *wasmVm, uint32_t functionIndex) {
  uint32_t currentStackPointer;
  if (virtualMemoryRead32(&wasmVm->callStack, 0, &currentStackPointer) != 0) {
    return -1;
  }

  // Read local variable count from function header
  uint32_t localCount;
  uint32_t bytesRead;
  uint32_t localOffset = wasmVm->codeState.functionTable[functionIndex]
    .codeOffset;
  bytesRead = readLeb128(&wasmVm->codeSegment, localOffset, &localCount);
  if (bytesRead == 0) {
    return -1;
  }

  // Calculate frame size
  uint32_t frameSize = wasmGetFrameSize(localCount);
  uint32_t frameStart = currentStackPointer + sizeof(uint32_t);

  // Write frame header fields
  if (virtualMemoryWrite32(&wasmVm->callStack,
    frameStart + offsetof(WasmStackFrame, functionIndex),
    functionIndex) != 0
  ) {
    return -1;
  }

  if (virtualMemoryWrite32(&wasmVm->callStack,
    frameStart + offsetof(WasmStackFrame, returnAddress),
    wasmVm->programCounter) != 0
  ) {
    return -1;
  }

  if (virtualMemoryWrite32(&wasmVm->callStack,
    frameStart + offsetof(WasmStackFrame, localCount),
    localCount) != 0
  ) {
    return -1;
  }

  // Get current operand stack pointer as frame's stack base
  uint32_t stackBase;
  if (virtualMemoryRead32(&wasmVm->globalStack, 0, &stackBase) != 0) {
    return -1;
  }

  if (virtualMemoryWrite32(&wasmVm->callStack,
    frameStart + offsetof(WasmStackFrame, stackBase),
    stackBase) != 0
  ) {
    return -1;
  }

  // Initialize locals to 0
  for (uint32_t ii = 0; ii < localCount; ii++) {
    if (virtualMemoryWrite32(&wasmVm->callStack,
      frameStart + offsetof(WasmStackFrame, locals) + (ii * sizeof(uint32_t)),
      0) != 0
    ) {
      return -1;
    }
  }

  // Write frame size after frame
  if (virtualMemoryWrite32(&wasmVm->callStack,
    frameStart + frameSize,
    frameSize) != 0
  ) {
    return -1;
  }

  // Update stack pointer
  currentStackPointer += sizeof(uint32_t) + frameSize + sizeof(uint32_t);
  if (virtualMemoryWrite32(&wasmVm->callStack, 0,
    currentStackPointer) != 0
  ) {
    return -1;
  }

  return 0;
}

/// @fn int32_t wasmPopFrame(WasmVm *wasmVm)
///
/// @brief Pop the top stack frame
///
/// @param wasmVm Pointer to the WASM VM state
///
/// @return Returns 0 on success, -1 on error
int32_t wasmPopFrame(WasmVm *wasmVm) {
  uint32_t currentStackPointer;
  if (virtualMemoryRead32(&wasmVm->callStack, 0, &currentStackPointer) != 0) {
    return -1;
  }

  // Check for underflow
  if (currentStackPointer < sizeof(uint32_t)) {
    return -1;
  }

  // Read frame size
  uint32_t frameSize;
  if (virtualMemoryRead32(&wasmVm->callStack,
    currentStackPointer - sizeof(uint32_t),
    &frameSize) != 0
  ) {
    return -1;
  }

  // Update stack pointer
  currentStackPointer -= (sizeof(uint32_t) + frameSize + sizeof(uint32_t));
  if (virtualMemoryWrite32(&wasmVm->callStack, 0,
    currentStackPointer) != 0
  ) {
    return -1;
  }

  return 0;
}

/// @fn int32_t wasmGetCurrentFrameBase(WasmVm *wasmVm, uint32_t *frameBase)
///
/// @brief Get the base address of the current stack frame
///
/// @param wasmVm Pointer to the WASM VM state
/// @param frameBase Pointer to store the frame base address
///
/// @return Returns 0 on success, -1 on error
int32_t wasmGetCurrentFrameBase(WasmVm *wasmVm, uint32_t *frameBase) {
  uint32_t currentStackPointer;
  if (virtualMemoryRead32(&wasmVm->callStack, 0, &currentStackPointer) != 0) {
    return -1;
  }

  // Check for empty stack
  if (currentStackPointer < sizeof(uint32_t)) {
    return -1;
  }

  // Read frame size
  uint32_t frameSize;
  if (virtualMemoryRead32(&wasmVm->callStack,
    currentStackPointer - sizeof(uint32_t),
    &frameSize) != 0
  ) {
    return -1;
  }

  *frameBase = currentStackPointer - sizeof(uint32_t) - frameSize;
  return 0;
}

/// @fn int32_t wasmGetLocal(WasmVm *wasmVm, uint32_t index, uint32_t *value)
///
/// @brief Get value of a local variable from current frame
///
/// @param wasmVm Pointer to the WASM VM state
/// @param index Index of the local variable
/// @param value Pointer to store the variable value
///
/// @return Returns 0 on success, -1 on error
int32_t wasmGetLocal(WasmVm *wasmVm, uint32_t index, uint32_t *value) {
  uint32_t frameBase;
  if (wasmGetCurrentFrameBase(wasmVm, &frameBase) != 0) {
    return -1;
  }

  // Read local count to validate index
  uint32_t localCount;
  if (virtualMemoryRead32(&wasmVm->callStack,
    frameBase + offsetof(WasmStackFrame, localCount),
    &localCount) != 0
  ) {
    return -1;
  }

  if (index >= localCount) {
    return -1;
  }

  return virtualMemoryRead32(&wasmVm->callStack,
    frameBase + offsetof(WasmStackFrame, locals) + (index * sizeof(uint32_t)),
    value);
}

/// @fn int32_t wasmSetLocal(WasmVm *wasmVm, uint32_t index, uint32_t value)
///
/// @brief Set value of a local variable in current frame
///
/// @param wasmVm Pointer to the WASM VM state
/// @param index Index of the local variable
/// @param value Value to set
///
/// @return Returns 0 on success, -1 on error
int32_t wasmSetLocal(WasmVm *wasmVm, uint32_t index, uint32_t value) {
  uint32_t frameBase;
  if (wasmGetCurrentFrameBase(wasmVm, &frameBase) != 0) {
    return -1;
  }

  // Read local count to validate index
  uint32_t localCount;
  if (virtualMemoryRead32(&wasmVm->callStack,
    frameBase + offsetof(WasmStackFrame, localCount),
    &localCount) != 0
  ) {
    return -1;
  }

  if (index >= localCount) {
    return -1;
  }

  return virtualMemoryWrite32(&wasmVm->callStack,
    frameBase + offsetof(WasmStackFrame, locals) + (index * sizeof(uint32_t)),
    value);
}

/// @fn int32_t wasmGetReturnAddress(WasmVm *wasmVm, uint32_t *returnAddress)
///
/// @brief Get return address from current frame
///
/// @param wasmVm Pointer to the WASM VM state
/// @param returnAddress Pointer to store the return address
///
/// @return Returns 0 on success, -1 on error
int32_t wasmGetReturnAddress(WasmVm *wasmVm, uint32_t *returnAddress) {
  uint32_t frameBase;
  if (wasmGetCurrentFrameBase(wasmVm, &frameBase) != 0) {
    return -1;
  }

  return virtualMemoryRead32(&wasmVm->callStack,
    frameBase + offsetof(WasmStackFrame, returnAddress),
    returnAddress);
}

/// @fn int32_t wasmGetFrameStackBase(WasmVm *wasmVm, uint32_t *stackBase)
///
/// @brief Get operand stack base from current frame
///
/// @param wasmVm Pointer to the WASM VM state
/// @param stackBase Pointer to store the stack base
///
/// @return Returns 0 on success, -1 on error
int32_t wasmGetFrameStackBase(WasmVm *wasmVm, uint32_t *stackBase) {
  uint32_t frameBase;
  if (wasmGetCurrentFrameBase(wasmVm, &frameBase) != 0) {
    return -1;
  }

  return virtualMemoryRead32(&wasmVm->callStack,
    frameBase + offsetof(WasmStackFrame, stackBase),
    stackBase);
}

/// @fn int wasmVmInit(WasmVm *wasmVm, const char *programPath,
///   const WasmImport *importTable, uint32_t importTableLength)
///
/// @brief Initialize all the state information for starting a WASI VM process.
///
/// @param wasmVm A pointer to the WasmVm state object maintained by the
///   wasmVmMain function.
/// @param programPath The full path to the WASI binary on disk.
///
/// @return Returns 0 on success, -1 on failure.
int wasmVmInit(WasmVm *wasmVm, const char *programPath,
  const WasmImport *importTable, uint32_t importTableLength
) {
  char tempFilename[13];
  int returnValue = 0;

  // Interleve virtual memory initializations and other operations that depend
  // on them in order to give variation to the filenames that get used (which
  // are based on time).

  returnValue = virtualMemoryInit(&wasmVm->codeSegment, programPath);

  if (returnValue == 0) {
    sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
    returnValue = virtualMemoryInit(&wasmVm->linearMemory, tempFilename);
  }

  if (returnValue == 0) {
    returnValue = wasmParseMemorySection(wasmVm);
  }

  if (returnValue == 0) {
    returnValue = wasmParseImports(wasmVm, importTable, importTableLength);
  }

  if (returnValue == 0) {
    sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
    returnValue = virtualMemoryInit(&wasmVm->globalStack, tempFilename);
  }

  if (returnValue == 0) {
    returnValue = wasmStackInit(&wasmVm->globalStack);
  }

  if (returnValue == 0) {
    sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
    returnValue = virtualMemoryInit(&wasmVm->callStack, tempFilename);
  }

  if (returnValue == 0) {
    returnValue = wasmStackInit(&wasmVm->callStack);
  }

  if (returnValue == 0) {
    sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
    returnValue = virtualMemoryInit(&wasmVm->globalStorage, tempFilename);
  }

  if (returnValue == 0) {
    returnValue = wasmFindStartFunction(wasmVm);
  }

  if (returnValue == 0) {
    sprintf(tempFilename, "%lu.mem", getElapsedMilliseconds(0) % 1000000000);
    returnValue = virtualMemoryInit(&wasmVm->tableSpace, tempFilename);
  }

  // wasmInitCodeState has to come last.
  if (returnValue == 0) {
    returnValue = wasmInitCodeState(wasmVm);
  }

  return returnValue;
}

/// @fn void wasmVmCleanup(WasmVm *wasmVm)
///
/// @brief Release all the resources allocated and held by a WASI VM state.
///
/// @param wasmVm A pointer to the WasmVm state object maintained by the
///   wasmVmMain function.
///
/// @return This function returns no value.
void wasmVmCleanup(WasmVm *wasmVm) {
  if (wasmVm->codeState.functionTable != NULL) {
    free(wasmVm->codeState.functionTable);
  }
  virtualMemoryCleanup(&wasmVm->tableSpace,    true);
  virtualMemoryCleanup(&wasmVm->globalStorage, true);
  virtualMemoryCleanup(&wasmVm->callStack,     true);
  virtualMemoryCleanup(&wasmVm->globalStack,   true);
  virtualMemoryCleanup(&wasmVm->linearMemory,  true);
  virtualMemoryCleanup(&wasmVm->codeSegment,   false);

  return;
}

