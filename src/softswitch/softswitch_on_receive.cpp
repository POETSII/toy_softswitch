#include "softswitch.hpp"

#include <cstdio>
#include <algorithm>

extern "C" void tinsel_puts(const char *);

//! Deal with the incoming message
extern "C" void softswitch_onReceive(PThreadContext *ctxt, const void *message)
{
  tinsel_puts("onReceive\n");
  
    const packet_t *packet=(const packet_t*)message;
    
    assert(packet); // Anything arriving via this route must have been a real packet (no init message via this route)

    softswitch_softswitch_log(4, "softswitch_onReceive /  dst=%08x:%04x:%02x, src=%08x:%04x:%02x", packet->dest.thread, packet->dest.device, packet->dest.port, packet->source.thread, packet->source.device, packet->source.port);
    
    
    ctxt->lamport=std::max(ctxt->lamport,packet->lamport)+1;
    
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
        
        softswitch_softswitch_log(4, "softswitch_onReceive / finding edge info, src=%08x:%04x:%02x, msg.laport=%u, propSize=%u, stateSize=%u", packet->source.thread, packet->source.device, packet->source.port, packet->lamport, port->propertiesSize , port->stateSize);
    
        
        const InputPortBinding *begin=dev->sources[portIndex].sourceBindings;
        const InputPortBinding *end=dev->sources[portIndex].sourceBindings+dev->sources[portIndex].numSources;
        
        // This will compile away into pure code, no library stuff
        auto edge=std::lower_bound(begin, end, packet->source, [](const InputPortBinding &a, const address_t &b){
            if(a.source.thread < b.thread) return true;
            if(a.source.thread > b.thread) return false;
            if(a.source.device < b.device) return true;
            if(a.source.device > b.device) return false;
            return a.source.port < b.port;
        });
        if(edge==end || edge->source.thread != packet->source.thread || edge->source.device != packet->source.device || edge->source.port != packet->source.port){
            softswitch_softswitch_log(0, "softswitch_onReceive / no edge found for packet : dst=%08x:%04x:%02x=%s, src=%08x:%04x:%02x", packet->dest.thread, packet->dest.device, packet->dest.port, dev->id, packet->source.thread, packet->source.device, packet->source.port);
            auto tmp=begin;
            while(tmp!=end){
                softswitch_softswitch_log(0, "softswitch_onReceive / possible src=%08x:%04x:%02x", tmp->source.thread, tmp->source.device, tmp->source.port);
                ++tmp;
            }
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
    softswitch_softswitch_log(4, "softswitch_onReceive / begin application handler, packet=%p, payload=%p", packet, packet->payload);
    
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
        packet->payload
    );

    ctxt->currentHandlerType=0;

    softswitch_softswitch_log(4, "softswitch_onReceive / end application handler\n");

    softswitch_softswitch_log(4, "softswitch_onReceive / updating RTS\n");
    softswitch_UpdateRTS(ctxt, dev);
    softswitch_softswitch_log(4, "softswitch_onReceive / new rts=%x\n", dev->rtsFlags);
    
    softswitch_softswitch_log(3, "softswitch_onReceive / end");
}
