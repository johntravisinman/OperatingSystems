#ifndef RESOURCE_DESCRIPTOR_H
#define RESOURCE_DESCRIPTOR_H

#include <stdio.h>
#include <stdlib.h>

#define MAX_PROCESSES 18
#define MAX_RESOURCES 20

typedef struct {
  // Matrix of requested resources and the requesting processes
  int requestMatrix[MAX_PROCESSES][MAX_RESOURCES];
  // Matrix of allocated resources and the processes holding them
  int allocationMatrix[MAX_PROCESSES][MAX_RESOURCES];
  // Vector of total existing resources
  int resourceVector[MAX_RESOURCES];
  // Vector of resources not being held
  int allocationVector[MAX_RESOURCES];
  // Vector to indicate if a resource is shared
  int sharedResourceVector[MAX_RESOURCES];
} resource_descriptor_t;

/* set given resource descriptor matrices values to 0  
 * resource vector values are random between 1 and 10 inclusively
 * allocation vector values are set to equal the resource vector as no 
 * resources will be allocated initially */
void init_resource_descriptor(resource_descriptor_t *descriptor) {
  int i, j;
  for (i = 0; i < MAX_PROCESSES; i++) {
    for (j = 0; j < MAX_RESOURCES; j++) {
      descriptor->requestMatrix[i][j] = 0;
      descriptor->allocationMatrix[i][j] = 0;
    }
  }
  for (i = 0; i < MAX_RESOURCES; i++) {
    if (rand() % 5 == 0) {
      descriptor->resourceVector[i] = 5;
      descriptor->allocationVector[i] = descriptor->resourceVector[i];
      descriptor->sharedResourceVector[i] = 1;
    } else {
      descriptor->resourceVector[i] = rand() % 10 + 1;
      descriptor->allocationVector[i] = descriptor->resourceVector[i];
      descriptor->sharedResourceVector[i] = 0;
    }
  }
  return;
}

//print a vector/array to given filestream
void print_vector(FILE *out, char *title, int vector[], int size) {
  int i;
  fprintf(out, "%-20s\n [ ", title);
  for (i = 0; i < size - 1; i++) {
    fprintf(out, "%2d, ", vector[i]);
  }
  fprintf(out, "%2d ]\n", vector[i]);
  return;
}
//print shared resource vector to given filestream
void print_shared_vector(FILE *out, char *title, int vector[], int size) {
  int i;
  fprintf(out, "%-20s\n   ", title);
  for (i = 0; i < size; i++) {
    if (vector[i] == 1) {
      fprintf(out, " *  ");
    } else {
      fprintf(out, "    ");
    }
  }
  fprintf(out, "\n");
  return;
}
//print a matrx/2d array to given filestream
void print_matrix(FILE *out, char *title, int matrix[MAX_PROCESSES][MAX_RESOURCES], int processes, int resources) {
  int col, row;
  fprintf(out, "%s\n    ", title);
  for (col = 0; col < resources; col++)
    fprintf(out, "R%-2d ", col);
  fprintf(out, "\n");
  for (row = 0; row < processes; row++) {
    fprintf(out, "P%-2d ", row);
    for (col = 0; col < resources; col++) {
      if (matrix[row][col] == 0) {
        fprintf(out, "--  ");
      } else {
        fprintf(out, "%-3d ", matrix[row][col]);
      }
    }
    fprintf(out, "\n");
  }
  return;
}
//Print resource descriptor to given filestream
void print_resource_descriptor(FILE *out, resource_descriptor_t descriptor, int resources, int processes) {
  print_vector(out, "Resource Vector", descriptor.resourceVector, resources);
  print_vector(out, "Allocation Vector", descriptor.allocationVector, resources);
  print_matrix(out, "\nRequest Matrix", descriptor.requestMatrix, processes, resources);
  print_matrix(out, "\nAllocation Matrix", descriptor.allocationMatrix, processes, resources);
  return;
}
#endif