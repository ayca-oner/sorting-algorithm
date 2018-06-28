#include "opendefs.h"
#include "openqueue.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "IEEE802154E.h"
#include "IEEE802154_security.h"

//=========================== defination =====================================

#define HIGH_PRIORITY_QUEUE_ENTRY 5

//=========================== variables =======================================

openqueue_vars_t openqueue_vars;

//=========================== prototypes ======================================
void openqueue_reset_entry(OpenQueueEntry_t* entry);

//=========================== public ==========================================


/*creator indicates the component which created this packet, i.e. which requested an unused OpenQueueEntry_t to the OpenQueue component. 
When sending a packet down the stack, the convention is that only the creator of a packet can free that packet, i.e. instruct the OpenQueue 
component it doesn't need it anymore. Typically, OpenQueueEntry_t variables will be created by all application-layer component when sending 
a packet, or by the drivers when receiving.

owner indicates the components which currently holds the packet. The convention is that a component can only change the content of an 
OpenQueueEntry_t if it is currently the owner.

The packet part of the OpenQueueEntry_t holds the actual bytes of the packet. Because OpenWSN does not allow for dynamic memory 
allocation, packet is the maximal allowed size. payload is a pointer to the first used by in packet; length indicates how many bytes are used.
*/


//======= admin

/**
\brief Initialize this module.
*/
void openqueue_init(void) {
   uint8_t i;
   for (i=0;i<QUEUELENGTH;i++){
      openqueue_reset_entry(&(openqueue_vars.queue[i]));
   }
}

//    resetting queue for every packet turn!!


/**
\brief Trigger this module to print status information, over serial.

debugPrint_* functions are used by the openserial module to continuously print
status information about several modules in the OpenWSN stack.

\returns TRUE if this function printed something, FALSE otherwise.
*/
bool debugPrint_queue(void) {
   debugOpenQueueEntry_t output[QUEUELENGTH];
   uint8_t i;
   for (i=0;i<QUEUELENGTH;i++) {
      output[i].creator = openqueue_vars.queue[i].creator;
      output[i].owner   = openqueue_vars.queue[i].owner;
   }
   openserial_printStatus(STATUS_QUEUE,(uint8_t*)&output,QUEUELENGTH*sizeof(debugOpenQueueEntry_t));
   return TRUE;
}


//======= called by any component

/**
\brief Request a new (free) packet buffer.

Component throughout the protocol stack can call this function is they want to
get a new packet buffer to start creating a new packet.

\note Once a packet has been allocated, it is up to the creator of the packet
      to free it using the openqueue_freePacketBuffer() function.

\returns A pointer to the queue entry when it could be allocated, or NULL when
         it could not be allocated (buffer full or not synchronized).
*/


//openqueue_getFreePacketBuffer returns a points to a unused OpenQueueEntry_t buffer, or NULL if they're all being used;


OpenQueueEntry_t* openqueue_getFreePacketBuffer(uint8_t creator) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   // refuse to allocate if we're not in sync
   if (ieee154e_isSynch()==FALSE && creator > COMPONENT_IEEE802154E){
     ENABLE_INTERRUPTS();
     return NULL;
   }
   
   // if you get here, I will try to allocate a buffer for you
   
   // if there is no space left for high priority queue, don't reserve
   if (openqueue_isHighPriorityEntryEnough()==FALSE && creator>COMPONENT_SIXTOP_RES){
      ENABLE_INTERRUPTS();
      return NULL;
   }
   
   // walk through queue and find free entry
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_NULL) {
         openqueue_vars.queue[i].creator=creator;
         openqueue_vars.queue[i].owner=COMPONENT_OPENQUEUE;
         openqueue_vars.queue[i].priority=8; //lowest priority is 8
         ENABLE_INTERRUPTS(); 
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}

OpenQueueEntry_t* openqueue_getFreePacketBuffer_withpriority(uint8_t creator, uint8_t priority ) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   
   // refuse to allocate if we're not in sync
   if (ieee154e_isSynch()==FALSE && creator > COMPONENT_IEEE802154E){
     ENABLE_INTERRUPTS();
     return NULL;
   }
   
   // if you get here, I will try to allocate a buffer for you
   
   // if there is no space left for high priority queue, don't reserve
   if (openqueue_isHighPriorityEntryEnough()==FALSE && creator>COMPONENT_SIXTOP_RES){
      ENABLE_INTERRUPTS();
      return NULL;
   }
   
   // walk through queue and find free entry
    for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_NULL) {
         openqueue_vars.queue[i].creator=creator;
         openqueue_vars.queue[i].owner=COMPONENT_OPENQUEUE;
         openqueue_vars.queue[i].priority=priority; //lowest priority is 8
         ENABLE_INTERRUPTS(); 
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}
 

/**
\brief Free a previously-allocated packet buffer.

\param pkt A pointer to the previsouly-allocated packet buffer.

\returns E_SUCCESS when the freeing was succeful.
\returns E_FAIL when the module could not find the specified packet buffer.
*/

//openqueue_freePacketBuffer instructs the module that this OpenQueueEntry_t buffer is no more needed; the module then frees it

owerror_t openqueue_freePacketBuffer(OpenQueueEntry_t* pkt) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (&openqueue_vars.queue[i]==pkt) {
         if (openqueue_vars.queue[i].owner==COMPONENT_NULL) {
            // log the error
            openserial_printCritical(COMPONENT_OPENQUEUE,ERR_FREEING_UNUSED,
                                  (errorparameter_t)0,
                                  (errorparameter_t)0);
         }
         openqueue_reset_entry(&(openqueue_vars.queue[i]));
         ENABLE_INTERRUPTS();
         return E_SUCCESS;
      }
   }
   // log the error
   openserial_printCritical(COMPONENT_OPENQUEUE,ERR_FREEING_ERROR,
                         (errorparameter_t)0,
                         (errorparameter_t)0);
   ENABLE_INTERRUPTS();
   return E_FAIL;
}

/**
\brief Free all the packet buffers created by a specific module.

\param creator The identifier of the component, taken in COMPONENT_*.
*/
void openqueue_removeAllCreatedBy(uint8_t creator) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++){
      if (openqueue_vars.queue[i].creator==creator) {
         openqueue_reset_entry(&(openqueue_vars.queue[i]));
      }
   }
   ENABLE_INTERRUPTS();
}

/**
\brief Free all the packet buffers owned by a specific module.

\param owner The identifier of the component, taken in COMPONENT_*.
*/
void openqueue_removeAllOwnedBy(uint8_t owner) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++){
      if (openqueue_vars.queue[i].owner==owner) {
         openqueue_reset_entry(&(openqueue_vars.queue[i]));
      }
   }
   ENABLE_INTERRUPTS();
}

//======= called by RES

OpenQueueEntry_t* openqueue_sixtopGetSentPacket(void) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_IEEE802154E_TO_SIXTOP &&
          openqueue_vars.queue[i].creator!=COMPONENT_IEEE802154E) {
         ENABLE_INTERRUPTS();
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}


OpenQueueEntry_t* openqueue_sixtopGetReceivedPacket(void) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_IEEE802154E_TO_SIXTOP &&
          openqueue_vars.queue[i].creator==COMPONENT_IEEE802154E) {
         ENABLE_INTERRUPTS();
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}


//======= called by IEEE80215E

OpenQueueEntry_t* openqueue_macGetDataPacket(open_addr_t* toNeighbor) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();

    uint8_t j, c, root, temp;

   for (i = 1; i < QUEUELENGTH; i++){
        c = i;
        do{
            root = (c - 1) / 2;             
            if (openqueue_vars.queue[root].priority < openqueue_vars.queue[c].priority){
                temp = openqueue_vars.queue[root].priority;
                openqueue_vars.queue[root].priority = openqueue_vars.queue[c].priority;
                openqueue_vars.queue[c].priority = temp;
            }
            c = root;
        } while (c != 0);
    }

    for (j = QUEUELENGTH - 1; j >= 0; j--){
        temp = openqueue_vars.queue[0].priority;
        openqueue_vars.queue[0].priority = openqueue_vars.queue[j].priority;  
        openqueue_vars.queue[j].priority = temp;
        root = 0;
        do{
            c = 2 * root + 1;    
            if ((openqueue_vars.queue[c].priority < openqueue_vars.queue[c + 1].priority) && c < j-1)
                c++;
            if (openqueue_vars.queue[root].priority<openqueue_vars.queue[c].priority && c<j){
                temp = openqueue_vars.queue[root].priority;
                openqueue_vars.queue[root].priority = openqueue_vars.queue[c].priority;
                openqueue_vars.queue[c].priority = temp;
            }
            root = c;
        } while (c < j);
    } 
//}

//from here the heap sort is complete, the queue is ordered from the smallest number to the highest
// 1st priority = 1 , 2nd priority = 2 , ...and so on
//after the first for it is not correctly sorted from high-to-low or low-to-high
//after the second for, the sorting is complete from low-to-high

// I wrote the code for sorting inside openqueue_macGetDataPacket so that the functions wouldn't get messed up
// and I wouldn't disrupt the actions in other files, the sorting will immediately be summoned when the function
// openqueue_macGetDataPacket is used

    // first to look the sixtop RES packet
    for (i=0;i<QUEUELENGTH;i++) {
       if (
           openqueue_vars.queue[i].owner==COMPONENT_SIXTOP_TO_IEEE802154E &&
           openqueue_vars.queue[i].creator==COMPONENT_SIXTOP_RES &&
           (
               (
                   toNeighbor->type==ADDR_64B &&
                   packetfunctions_sameAddress(toNeighbor,&openqueue_vars.queue[i].l2_nextORpreviousHop)
               ) || toNeighbor->type==ADDR_ANYCAST
           )
       ){
          ENABLE_INTERRUPTS();
          return &openqueue_vars.queue[i];
       }
    }
  
   if (toNeighbor->type==ADDR_64B) {
      // a neighbor is specified, look for a packet unicast to that neigbhbor
      for (i=0;i<QUEUELENGTH;i++) {
         if (openqueue_vars.queue[i].owner==COMPONENT_SIXTOP_TO_IEEE802154E &&
            packetfunctions_sameAddress(toNeighbor,&openqueue_vars.queue[i].l2_nextORpreviousHop)
          ) {
            ENABLE_INTERRUPTS();
            return &openqueue_vars.queue[i];
         }
      }
   } else if (toNeighbor->type==ADDR_ANYCAST) {
      // anycast case: look for a packet which is either not created by RES
      // or an KA (created by RES, but not broadcast)
      for (i=0;i<QUEUELENGTH;i++) {
         if (openqueue_vars.queue[i].owner==COMPONENT_SIXTOP_TO_IEEE802154E &&
             ( openqueue_vars.queue[i].creator!=COMPONENT_SIXTOP ||
                (
                   openqueue_vars.queue[i].creator==COMPONENT_SIXTOP &&
                   packetfunctions_isBroadcastMulticast(&(openqueue_vars.queue[i].l2_nextORpreviousHop))==FALSE
                )
             )
            ) {
            ENABLE_INTERRUPTS();
            return &openqueue_vars.queue[i];
         }
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}

bool openqueue_isHighPriorityEntryEnough(void) {
    uint8_t i;
    uint8_t numberOfEntry;
    
    numberOfEntry = 0;
    for (i=0;i<QUEUELENGTH;i++) {
        if(openqueue_vars.queue[i].creator>COMPONENT_SIXTOP_RES){
            numberOfEntry++;
        }
    }
    
    if (numberOfEntry>QUEUELENGTH-HIGH_PRIORITY_QUEUE_ENTRY){
        return FALSE;
    } else {
        return TRUE;
    }
}

OpenQueueEntry_t* openqueue_macGetEBPacket(void) {
   uint8_t i;
   INTERRUPT_DECLARATION();
   DISABLE_INTERRUPTS();
   for (i=0;i<QUEUELENGTH;i++) {
      if (openqueue_vars.queue[i].owner==COMPONENT_SIXTOP_TO_IEEE802154E &&
          openqueue_vars.queue[i].creator==COMPONENT_SIXTOP              &&
          packetfunctions_isBroadcastMulticast(&(openqueue_vars.queue[i].l2_nextORpreviousHop))) {
         ENABLE_INTERRUPTS();
         return &openqueue_vars.queue[i];
      }
   }
   ENABLE_INTERRUPTS();
   return NULL;
}

//=========================== private =========================================

void openqueue_reset_entry(OpenQueueEntry_t* entry) {
   //admin
   entry->creator                      = COMPONENT_NULL;
   entry->owner                        = COMPONENT_NULL;
   entry->payload                      = &(entry->packet[127 - IEEE802154_SECURITY_TAG_LEN]); // Footer is longer if security is used
   entry->length                       = 0;
   //l4
   entry->l4_protocol                  = IANA_UNDEFINED;
   entry->l4_protocol_compressed       = FALSE;
   //l3
   entry->l3_destinationAdd.type       = ADDR_NONE;
   entry->l3_sourceAdd.type            = ADDR_NONE;
   //l2
   entry->l2_nextORpreviousHop.type    = ADDR_NONE;
   entry->l2_frameType                 = IEEE154_TYPE_UNDEFINED;
   entry->l2_retriesLeft               = 0;
   entry->l2_IEListPresent             = 0;
   entry->l2_isNegativeACK             = 0;
   entry->l2_payloadIEpresent          = 0;
   //l2-security
   entry->l2_securityLevel             = 0;
}
