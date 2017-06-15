#include "softswitch.hpp"

#include <cstdio>
#include <algorithm>

extern "C" void tinsel_puts(const char *);

//! Deal with the incoming message
extern "C" void softswitch_onReceive(PThreadContext *ctxt, const void *message)
{ 
    const packet_t *packet=(const packet_t*)message;
    const void *payload=(const char*)(packet+1);
    
    assert(packet); // Anything arriving via this route must have been a real packet (no init message via this route)

    softswitch_softswitch_log(4, "softswitch_onReceive /  dst=%08x:%04x:%02x, src=%08x:%04x:%02x, size=%x", packet->dest.thread, packet->dest.device, packet->dest.port, packet->source.thread, packet->source.device, packet->source.port, packet->size);

    assert( packet->size >= sizeof(packet_t) );
    softswitch_softswitch_log(5, "softswitch_onReceive /  packet->size=%x, sizeof(packet_t)=%x", packet->size, sizeof(packet_t));

    int payloadSize = packet->size - (int)sizeof(packet_t);
    assert(payloadSize>=0);
    for(int i=0; i< payloadSize; i++){
      softswitch_softswitch_log(5, "softswitch_onReceive /   payload[%u]=%x", i, ((uint8_t*)payload)[i]);
    }
    
    
    // Map to the device and its vtable
    unsigned deviceIndex=packet->dest.device;
    softswitch_softswitch_log(4, "softswitch_onReceive / finding vtable, device inst index=%u", deviceIndex);
    assert( deviceIndex < ctxt->numDevices );
    DeviceContext *dev=ctxt->devices + deviceIndex;
    const DeviceTypeVTable *vtable=dev->vtable;
    softswitch_softswitch_log(4, "softswitch_onReceive / device inst id=%s, type=%s", dev->id, vtable->id);
        
    // Map to the particular port
    unsigned portIndex=packet->dest.port;
    softswitch_softswitch_log(4, "softswitch_onReceive / finding port index=%u on device type=%s (vtable->numInput=%d)", portIndex, vtable->id, vtable->numInputs);
    assert(portIndex < vtable->numInputs);
    const InputPortVTable *port=vtable->inputPorts + portIndex;
    receive_handler_t handler=port->receiveHandler;
    
    const void *eProps=0;
    void *eState=0;
    
    if(port->propertiesSize | port->stateSize){
        // We have to look up the edge info associated with this edge
        
        softswitch_softswitch_log(4, "softswitch_onReceive / finding edge info, src=%08x:%04x:%02x, propSize=%u, stateSize=%u", packet->source.thread, packet->source.device, packet->source.port, port->propertiesSize , port->stateSize);
    
        
        const InputPortBinding *begin=dev->sources[portIndex].sourceBindings;
        const InputPortBinding *end=dev->sources[portIndex].sourceBindings+dev->sources[portIndex].numSources;

	auto cmpAddress=[](const address_t &a, const address_t &b) -> bool {
	  if(a.thread < b.thread) return true;
	  if(a.thread > b.thread) return false;
	  if(a.device < b.device) return true;
	  if(a.device > b.device) return false;
	  return a.port < b.port;
	};
        
        // This will compile away into pure code, no library stuff
        auto edge=std::lower_bound(begin, end, packet->source, [=](const InputPortBinding &a, const address_t &b) -> bool{
            return cmpAddress(a.source,b);
        });
        if(edge==end || edge->source.thread != packet->source.thread || edge->source.device != packet->source.device || edge->source.port != packet->source.port){
            softswitch_softswitch_log(0, "softswitch_onReceive / no edge found for packet : dst=%08x:%04x:%02x=%s, src=%08x:%04x:%02x", packet->dest.thread, packet->dest.device, packet->dest.port, dev->id, packet->source.thread, packet->source.device, packet->source.port);
            auto tmp=begin;
            while(tmp!=end){
                softswitch_softswitch_log(0, "softswitch_onReceive / possible src=%08x:%04x:%02x", tmp->source.thread, tmp->source.device, tmp->source.port);
		if(begin+1 != tmp){
		  assert(cmpAddress((tmp-1)->source, tmp->source));
		}
		
		++tmp;
            }
	    assert(std::is_sorted(begin, end, [=](const InputPortBinding &a, const InputPortBinding &b) -> bool {
		  return cmpAddress(a.source,b.source);
		}));
	    

            assert(0);
        }
        
        
        softswitch_softswitch_log(4, "softswitch_onReceive / found edge, src=%08x:%04x:%02x=%u", edge->source.thread, edge->source.device, edge->source.port);
    
        
        assert(edge->source.port==packet->source.port);
        assert(edge->source.device==packet->source.device);
        assert(edge->source.thread==packet->source.thread);
        
        
        eProps=edge->edgeProperties;
        eState=edge->edgeState;
        // If an edge has properties or state, you must deliver some
        assert(!port->propertiesSize || eProps);
        assert(!port->stateSize || eState);
    }
    
            
    // Call the application level handler
    softswitch_softswitch_log(4, "softswitch_onReceive / begin application handler, packet=%p, size=%u", packet, packet->size);
    
    // Needed for handler logging
    ctxt->currentDevice=deviceIndex;
    ctxt->currentHandlerType=1;
    ctxt->currentPort=portIndex;

    handler(
        ctxt->graphProps,
        dev->properties,
        dev->state,
        eProps,     
        eState,     
        (void *)(packet+1)
    );

    ctxt->currentHandlerType=0;

    softswitch_softswitch_log(4, "softswitch_onReceive / end application handler\n");

    softswitch_softswitch_log(4, "softswitch_onReceive / updating RTS\n");
    softswitch_UpdateRTS(ctxt, dev);
    softswitch_softswitch_log(4, "softswitch_onReceive / new rts=%x\n", dev->rtsFlags);
    
    softswitch_softswitch_log(3, "softswitch_onReceive / end");
}
