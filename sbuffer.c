/**
 * \author Mathieu Erbas
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include "datamgr.h"
#endif

#include "sbuffer.h"
#include "sensor_node.c"

#include "config.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

typedef struct sbuffer_node {
    struct sbuffer_node* prev;
    sensor_data_t data;
    bool read;
} sbuffer_node_t;

struct sbuffer {
    sbuffer_node_t* head;
    sbuffer_node_t* tail;
    sbuffer_node_t* dataPointers[2];    // datamngrPointer, storemngrPointer
    bool closed;

    int added;
    int stored;
    int calculated;

    pthread_mutex_t mutex;
    pthread_cond_t conditie;
};

static sbuffer_node_t* create_node(const sensor_data_t* data) {
    sbuffer_node_t* node = malloc(sizeof(*node));
    *node = (sbuffer_node_t){
        .data = *data,
        .prev = NULL,
        .read = false,
    };
    return node;
}

sbuffer_t* sbuffer_create() {
    sbuffer_t* buffer = malloc(sizeof(sbuffer_t));
    // should never fail due to optimistic memory allocation
    assert(buffer != NULL);

    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->dataPointers[0] = NULL;
    buffer->dataPointers[1] = NULL;
    buffer->closed = false;

    buffer->added=0;
    buffer->stored=0;
    buffer->calculated=0;
    
    ASSERT_ELSE_PERROR(pthread_mutex_init(&buffer->mutex, NULL) == 0);
    ASSERT_ELSE_PERROR(pthread_cond_init(&buffer->conditie, NULL) == 0);

    return buffer;
}

void bufferAddData(sbuffer_t* buffer){
    buffer->calculated++;
}
void bufferAddStored(sbuffer_t* buffer){
    buffer->stored++;
}
int getAdded(sbuffer_t* buffer){
    return buffer->added;
}
int getData(sbuffer_t* buffer){
    return buffer->calculated;
}
int getStored(sbuffer_t* buffer){
    return buffer->stored;
}

void sbuffer_destroy(sbuffer_t* buffer) {
    assert(buffer);
    // make sure it's empty
    assert(buffer->head == buffer->tail);
    ASSERT_ELSE_PERROR(pthread_mutex_destroy(&buffer->mutex) == 0);
    ASSERT_ELSE_PERROR(pthread_cond_destroy(&buffer->conditie) == 0);
    free(buffer);
}

void sbuffer_lock(sbuffer_t* buffer) {
    assert(buffer);
    ASSERT_ELSE_PERROR(pthread_mutex_lock(&buffer->mutex) == 0);
}
void sbuffer_unlock(sbuffer_t* buffer) {
    assert(buffer);
    ASSERT_ELSE_PERROR(pthread_mutex_unlock(&buffer->mutex) == 0);
}

int sbuffer_insert_first(sbuffer_t* buffer, sensor_data_t const* data) {
    //lock 
    sbuffer_lock(buffer);

    assert(buffer && data);

    // create new node
    sbuffer_node_t* node = create_node(data);
    assert(node->prev == NULL);


    // insert it
    if (buffer->head != NULL)
        buffer->head->prev = node;
    buffer->head = node;
    if (buffer->tail == NULL)
        buffer->tail = node;
    
    // threads wakker maken
    if(buffer->dataPointers[0] == NULL || buffer->dataPointers[1] == NULL)
        pthread_cond_broadcast(&buffer->conditie);

    // pointer op node zetten indien reader mee is met data
    if(buffer->dataPointers[0] == NULL)
        buffer->dataPointers[0] = node;
    if(buffer->dataPointers[1] == NULL)
        buffer->dataPointers[1] = node;
    
    //unlock
    sbuffer_unlock(buffer);
    buffer->added++;

    return SBUFFER_SUCCESS;
}

sensor_data_t sbuffer_remove_last(sbuffer_t* buffer) {
    assert(buffer);
    assert(buffer->head != NULL);

    sbuffer_node_t* removed_node = buffer->tail;
    assert(removed_node != NULL);
    if (removed_node == buffer->head) {
        buffer->head = NULL;
        assert(removed_node == buffer->tail);
    }
    buffer->tail = removed_node->prev;
    sensor_data_t ret = removed_node->data;
    free(removed_node);

    return ret;
}

void sbuffer_close(sbuffer_t* buffer) {
    //lock
    sbuffer_lock(buffer);

    assert(buffer);
    buffer->closed = true;
    pthread_cond_broadcast(&buffer->conditie);

    //unlock
    sbuffer_unlock(buffer);
}

sensor_data_t *sbuffer_dataProcessmngr(sbuffer_t* buffer, int reader){
    //lock 
    sbuffer_lock(buffer);

    while(buffer->dataPointers[reader]==NULL && !buffer->closed)
        pthread_cond_wait(&buffer->conditie, &buffer->mutex);
    if(buffer->closed && buffer->dataPointers[reader]==NULL){
        //assert(buffer->dataPointers[reader]==NULL);
        return NULL;
    }

    sensor_data_t *data = &(buffer->dataPointers[reader]->data);

    //unlock 
    sbuffer_unlock(buffer);

    return data;
}

void sbuffer_mark_read(sbuffer_t* buffer, int reader){
    //lock
    sbuffer_lock(buffer);

    sbuffer_node_t* node = buffer->dataPointers[reader];
    buffer->dataPointers[reader] = buffer->dataPointers[reader]->prev;
    if(node->read)
        sbuffer_remove_last(buffer);
    else
        node->read = true;

    //unlock
    sbuffer_unlock(buffer);
}
