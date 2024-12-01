#include "vm.h"

#define FALSE 0
#define TRUE 1

// Node structure to represent a block of allocated memory
typedef struct Node {
    size_t size;
    int used;
    struct Node* next;
    struct Node* prev;
} Node;


// Function to initialize the memory manager
void initialize_memory_manager(LPVM vm, void* buffer, size_t buffer_size) {
    // Initialize the linked list with a single node representing the entire buffer
    vm->head = (Node*)buffer;
    vm->head->size = buffer_size - sizeof(Node);
    vm->head->next = NULL;
    vm->head->prev = NULL;
    vm->head->used = FALSE;
}

// Function to allocate memory from the buffer
void* my_malloc(LPVM vm, size_t size) {
    Node* current = vm->head;
    
    // Traverse the linked list to find a suitable block
    while (current != NULL) {
        if (!current->used && current->size >= size) {
            // Allocate memory from the current block
            if (current->size > size + sizeof(Node)) {
                // If there's enough space, create a new node for the remaining block
                Node* new_block = (Node*)((char*)current + sizeof(Node) + size);
                new_block->size = current->size - size - sizeof(Node);
                new_block->next = current->next;
                new_block->prev = current;
                new_block->used = FALSE;
                if (current->next != NULL) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
            }
            // Update the current block size and return the allocated memory
            current->size = size;
            current->used = TRUE;
            return (void*)(current + 1); // Skip the Node header
        }
        // Move to the next block
        current = current->next;
    }
    
    fprintf(stderr, "VM: Out of memory for block of %zu bytes\n", size);
    // No suitable block found
    return NULL;
}

// Function to free previously allocated memory
void my_free(LPVM vm, void* ptr) {
    if (ptr == NULL) {
        // Ignore freeing NULL pointer
        return;
    }
    
    // Find the Node header before the given pointer
    Node* node = ((Node*)ptr) - 1;
    
    node->used = FALSE;
    
    if (node->prev && !node->prev->used) {
        node->prev->next = node->next;
        if (node->next) {
            node->next->prev = node->prev;
        }
        node->prev->size += node->size + sizeof(Node);
        node = node->prev;
    }
    
    if (node->next && !node->next->used) {
        node->next->prev = node->prev;
        if (node->prev) {
            node->prev->next = node->next;
        }
        node->next->size += node->size + sizeof(Node);
        node = node->next;
    }
}
