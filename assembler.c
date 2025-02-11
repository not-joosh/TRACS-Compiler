 /*======================================================================================================
* FILE        : assembler.c
* AUTHOR      : Josh Ratificar (Hardware Lead)
*               Ben Cesar Cadungog (Software Lead)
*               Jeddah Laine Luce�ara  (Research Lead)
*               Harold Marvin Comendador (Documentation Lead)
* DESCRIPTION : This file contains the implementation of the functions that assemble the script.asm file.
* COPYRIGHT   : 23 April, 2024
* REVISION HISTORY:
*   21 April, 2024: V1.0 - File Created
*   22 April, 2024: V1.1 - Added the process_file function to read and format the assembly code
*   23 April, 2024, V2.0 - Implemented Logic for label addressing, opcode conversion, and output redirection.
*   24 April, 2024, V2.1 - Added error handling for invalid labels and instructions. Created Readme file.
*   25 April, 2024, V2.2 - Finalized Issue with handling operand that needs to be added with the opcode.
*   25 April, 2024, V2.3 - Added error handling invalid operation for instruction. 
======================================================================================================*/
/*===============================================
 *   HEADER FILES
 *==============================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include "assembler.h"

/*===============================================
*   FUNCTION    :   assemble
*   DESCRIPTION :   This function assembles the script.asm file into formatted TRACS "C" code.
*   ARGUMENTS   :   bool *success, char **lines, const char *filename
*   RETURNS     :   int
 *==============================================*/
int assemble(void) {
    // Initialization...
    int success = 0;
    unsigned int address = 0x000;
    unsigned int temp_address = 0x000;
    bool hasEOP = false;
    int label_count = 0;
    int line_count;
    LABEL labels[MAX_LINES];

    // Step 1: Read the assembly code from the array and store it in an array of LINE structs
    LINE *lines = process_file("script.asm", &line_count);
    if (lines == NULL) {
        printf("Error reading lines\n");
        return success;
    }

    // Printing all the lines 
    for (int i = 0; i < line_count; i++)
    {
        printf("Label: %s, Operation: %s, Operand: %s\n", lines[i].label, lines[i].operation, lines[i].operand);
    }
    getchar();
    // Step 2: If set, load address, else set to 0x000
    set_address(&address, line_count, lines);
    temp_address = address; // Saving address to temp_address for later use (Labels)

    // Step 3: Create a label array and populate it with labels and their addresses
    for (int i = 1; i < line_count; i++) {
        OPOBJ op = get_opcode(lines[i].operation);
        if(strcmp(lines[i].label, "EOP") == 0 || strcmp(lines[i].operation, "EOP") == 0) 
            hasEOP = true;
        if (lines[i].label[0] != '\0') {
            strcpy(labels[label_count].label, lines[i].label);
            labels[label_count].address = temp_address;
            label_count++;
        }
        temp_address += 2; // Increment the address for each line
    }
    if(!hasEOP) 
    {
        printf("Error: No EOP found\n");
        free(lines);
        return success;
    }

    // Step 4: Check for invalid labels as operands...
    bool hasInvalidLabel = false;
    for (int i = 1; i < line_count; i++) {
        if (lines[i].operand[0] != '\0' && strncmp(lines[i].operand, "0x", 2) != 0) {
            int labelFound = 0;
            for (int j = 0; j < label_count; j++) {
                if (strcmp(lines[i].operand, labels[j].label) == 0) {
                    labelFound = 1;
                    break;
                }
            }
            if (!labelFound) {
                hasInvalidLabel = true;
                printf("Error: Unknown Label: %s\n", lines[i].operand);
            }
        }
    }

    if(hasInvalidLabel) {
        free(lines);
        return success;
    }

    bool hasInvalidOperation = false;
    for (int i = 1; i < line_count; i++) {
        OPOBJ op = get_opcode(lines[i].operation);
        if (op.opcode == -1) 
        {
            printf("Invalid instruction: %s\n", lines[i].label);
            hasInvalidOperation = true;
        }
        // There is also another condition, only BR, BRE, BRNE, BRGT, BRLT can have labels as operands
        if(strcmp(lines[i].operation, "BR") != 0 && strcmp(lines[i].operation, "BRE") != 0 && strcmp(lines[i].operation, "BRNE") != 0 && strcmp(lines[i].operation, "BRGT") != 0 && strcmp(lines[i].operation, "BRLT") != 0)
        {
            if(lines[i].operand[0] != '\0' && strncmp(lines[i].operand, "0x", 2) != 0)
            {
                int labelFound = 0;
                for (int j = 0; j < label_count; j++) {
                    if (strcmp(lines[i].operand, labels[j].label) == 0) {
                        labelFound = 1;
                        break;
                    }
                }
                if (labelFound) {
                    printf("Error: Invalid operand for instruction: %s\n", lines[i].operation);
                    hasInvalidOperation = true;
                }
            }
        }
    }
    if(hasInvalidOperation) {
        free(lines);
        return success;
    }
        
    // Open file for writing...
    FILE *output_file = fopen("translation.txt", "w"); 
    if (output_file == NULL) {
        printf("Error opening output file\n");
        free(lines);
        return success;
    }

    // Print formatted TRACS code...
    for (int i = 1; i < line_count; i++) {
        bool flag = false;
        OPOBJ op = get_opcode(lines[i].operation);
        if (op.opcode == -1) 
        {
            printf("Invalid instruction: %s\n", lines[i].operation);
            continue;
        }
        int concat = 0x0000;
        if(op.addBoolean)
        {
            op.opcode = op.opcode << 8;
            unsigned long operand_int = strtoul(lines[i].operand + 2, NULL, 16); // Skip "0x" prefix
            concat = op.opcode | operand_int; 
            int first = (concat >> 8) & 0xFF;
            int second = concat & 0xFF;
            // Checking to see if it is a branch operation

            if(strcmp(lines[i].operation, "BR") == 0 || strcmp(lines[i].operation, "BRE") == 0 || strcmp(lines[i].operation, "BRNE") == 0 || strcmp(lines[i].operation, "BRGT") == 0 || strcmp(lines[i].operation, "BRLT") == 0)
            {
                // We know that the branch points to a label. So instead of second, we will point to the label address
                // Looping through each label to find the address of the label. We compare label name
                for (int j = 0; j < label_count; j++) 
                {
                    if (strcmp(lines[i].operand, labels[j].label) == 0) {
                        fprintf(output_file, "0x%02x 0x%02x\t0x%02x 0x%02x\n", address, first, address + 1, labels[j].address);
                        break;
                    }
                }
            }
            else
            {
                fprintf(output_file, "0x%02x 0x%02x\t0x%02x 0x%02x\n", address, first, address + 1, second);
            }
        }
        else
        {
            flag = true;
            fprintf(output_file, "0x%02x 0x%02x\t", address, op.opcode);
        }
        if(flag)
        {
            int labelFound = 0;
            for (int j = 0; j < label_count; j++) {
                if (strcmp(lines[i].operand, labels[j].label) == 0) {
                    fprintf(output_file, "0x%02x 0x%02x\n", address + 1, labels[j].address);
                    labelFound = 1;
                    break;
                }
            }
            if (!labelFound && lines[i].operand[0] == '\0') {
                fprintf(output_file, "0x%02x 0x00\n", address + 1);
                labelFound = 1;
            }
            if (!labelFound && strncmp(lines[i].operand, "0x", 2) == 0) {
                int k = 2;
                while (lines[i].operand[k] != '\0') {
                    if (!isdigit(lines[i].operand[k])) {
                        break;
                    }
                    k++;
                }
                if (lines[i].operand[k] == '\0') {
                    fprintf(output_file, "0x%02x %s\n", address + 1, lines[i].operand);
                    labelFound = 1;
                }
            }
            if (!labelFound) {
                fprintf(output_file, "Unknown Label: %s Writing opcode %s\n", lines[i].operand, lines[i].operation);
            }
        }
        address += 2;
    }

    // Close the output file
    fclose(output_file);
    // Free allocated memory...
    free(lines);
    return 1;
}

/*===============================================
*   FUNCTION    :   set_address
*   DESCRIPTION :   This function will set the address of the instruction.
*   ARGUMENTS   :   unsigned int *address
*   RETURNS     :   VOID
 *==============================================*/
void set_address(unsigned int *address, int line_count, LINE *lines)
{
    bool orgFound = false;
    for (int i = 0; i < line_count; i++)
    {
        if (strcmp(lines[i].label, "ORG") == 0)
        {
            orgFound = true;
            *address = strtol(lines[i].operation, NULL, 0);
            return;
        }
    }
    if(orgFound == false)
        *address = 0x000;
}

/*===============================================
*   FUNCTION    :   process_file
*   DESCRIPTION :   This function will read the assembly code from the file and store it in an array of LINE structs.
*   ARGUMENTS   :   const char *filename, int *line_count
*   RETURNS     :   LINE *
 *==============================================*/
LINE* process_file(const char *filename, int *line_count) {
    // Open the file
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error opening file %s\n", filename);
        return NULL;
    }

    // Allocate memory for the array of lines
    LINE *lines = malloc(MAX_LINES * sizeof(LINE));
    if (lines == NULL) {
        printf("Memory allocation failed\n");
        fclose(fp);
        return NULL;
    }

    // Read the file LINE by LINE
    char LINE[MAX_LINE_LENGTH];
    *line_count = 0;
    while (fgets(LINE, sizeof(LINE), fp))
    {
        // Remove comments
        char *comment = strchr(LINE, ';');
        if (comment != NULL)
            *comment = '\0'; // Terminate the string at the comment position

        // Remove leading and trailing whitespace
        char *start = LINE;
        while (*start == ' ' || *start == '\t') start++;
        char *end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
        *(end + 1) = '\0';

        // Skip empty lines
        if (strlen(start) == 0)
            continue;

        // Parse the LINE and store it in the array
        if (strlen(start) > 0)
        {
            sscanf(start, "%s %s %s", lines[*line_count].label, lines[*line_count].operation, lines[*line_count].operand);
            (*line_count)++;
        }
    }

    // Close the file
    fclose(fp);

    // Format Correction
    for (int i = 0; i < *line_count; i++)
    {
        if (lines[i].label[0] != '\0' && (
                strcmp(lines[i].label, "WM") == 0 || strcmp(lines[i].label, "RM") == 0 ||
                strcmp(lines[i].label, "WACC") == 0 || strcmp(lines[i].label, "WIB") == 0 ||
                strcmp(lines[i].label, "WIO") == 0 || strcmp(lines[i].label, "RACC") == 0 ||
                strcmp(lines[i].label, "ADD") == 0 || strcmp(lines[i].label, "SUB") == 0 ||
                strcmp(lines[i].label, "MUL") == 0 || strcmp(lines[i].label, "AND") == 0 ||
                strcmp(lines[i].label, "OR") == 0 || strcmp(lines[i].label, "NOT") == 0 ||
                strcmp(lines[i].label, "XOR") == 0 || strcmp(lines[i].label, "SHL") == 0 ||
                strcmp(lines[i].label, "SHR") == 0 || strcmp(lines[i].label, "BR") == 0 ||
                strcmp(lines[i].label, "BRE") == 0 || strcmp(lines[i].label, "BRNE") == 0 ||
                strcmp(lines[i].label, "BRGT") == 0 || strcmp(lines[i].label, "BRLT") == 0 ||
                strcmp(lines[i].label, "EOP") == 0 || strcmp(lines[i].label, "SWAP") == 0 ||
                strcmp(lines[i].label, "WB") == 0
            ))
        {
            // This means it's not a label, so move everything to the right
            strcpy(lines[i].operand, lines[i].operation);
            strcpy(lines[i].operation, lines[i].label);
            strcpy(lines[i].label, "");
        }
    }
    
    // Remove lines with empty label, operation, and operand fields
    int index = 0;
    for (int i = 0; i < *line_count; i++)
    {
        if (lines[i].label[0] != '\0' || lines[i].operation[0] != '\0' || lines[i].operand[0] != '\0')
        {
            if (i != index)
            {
                strcpy(lines[index].label, lines[i].label);
                strcpy(lines[index].operation, lines[i].operation);
                strcpy(lines[index].operand, lines[i].operand);
            }
            index++;
        }
    }
    *line_count = index;

    return lines;
}

/*===============================================
*   FUNCTION    :   get_opcode
*   DESCRIPTION :   This function will return the opcode based on the instruction.
*   ARGUMENTS   :   char *instruction
*   RETURNS     :   char *
 *==============================================*/
OPOBJ get_opcode(char *instruction) 
{ 
    OPOBJ op;
    if (strcmp(instruction, "WB") == 0)             {op.opcode = 0x30; op.addBoolean = false;}
    else if (strcmp(instruction, "WM") == 0)        {op.opcode = 0x08; op.addBoolean = true;}
    else if (strcmp(instruction, "RM") == 0)        {op.opcode = 0x10; op.addBoolean = true;}
    else if (strcmp(instruction, "WACC") == 0)      {op.opcode = 0x48; op.addBoolean = false;}
    else if (strcmp(instruction, "WIB") == 0)       {op.opcode = 0x38; op.addBoolean = false;}
    else if (strcmp(instruction, "WIO") == 0)       {op.opcode = 0x28; op.addBoolean = true;}
    else if (strcmp(instruction, "RACC") == 0)      {op.opcode = 0x58; op.addBoolean = false;}
    else if (strcmp(instruction, "ADD") == 0)       {op.opcode = 0xF0; op.addBoolean = false;}
    else if (strcmp(instruction, "SUB") == 0)       {op.opcode = 0xE8; op.addBoolean = false;}
    else if (strcmp(instruction, "MUL") == 0)       {op.opcode = 0xD8; op.addBoolean = false;}
    else if (strcmp(instruction, "AND") == 0)       {op.opcode = 0xD0; op.addBoolean = false;}
    else if (strcmp(instruction, "OR") == 0)        {op.opcode = 0xC8; op.addBoolean = false;}
    else if (strcmp(instruction, "NOT") == 0)       {op.opcode = 0xC0; op.addBoolean = false;}
    else if (strcmp(instruction, "XOR") == 0)       {op.opcode = 0xB8; op.addBoolean = false;}
    else if (strcmp(instruction, "SHL") == 0)       {op.opcode = 0xB0; op.addBoolean = false;}
    else if (strcmp(instruction, "SHR") == 0)       {op.opcode = 0xA8; op.addBoolean = false;}
    else if (strcmp(instruction, "BR") == 0)        {op.opcode = 0x18; op.addBoolean = true;}
    else if (strcmp(instruction, "BRE") == 0)       {op.opcode = 0xA0; op.addBoolean = true;}
    else if (strcmp(instruction, "BRNE") == 0)      {op.opcode = 0x98; op.addBoolean = true;}
    else if (strcmp(instruction, "BRGT") == 0)      {op.opcode = 0x90; op.addBoolean = true;}
    else if (strcmp(instruction, "BRLT") == 0)      {op.opcode = 0x88; op.addBoolean = true;}
    else if (strcmp(instruction, "EOP") == 0)       {op.opcode = 0xF8; op.addBoolean = false;}
    else if (strcmp(instruction, "SWAP") == 0)      {op.opcode = 0x70; op.addBoolean = false;}
    else op.opcode = -1; // Indicates invalid opcode
    
    return op;
}
/*===============================================
*   FUNCTION    :   printLabels
*   DESCRIPTION :   This function will print the labels and their addresses.
*   ARGUMENTS   :   int label_count, LABEL labels
*   RETURNS     :   VOID
 *==============================================*/
void printLabels(int label_count, LABEL *labels)
{
    for (int i = 0; i < label_count; i++)
    {
        printf("Label: %s, Address: %x\n", labels[i].label, labels[i].address);
    }
}
