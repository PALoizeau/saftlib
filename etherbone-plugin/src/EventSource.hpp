#ifndef EB_PLUGIN_EVENTSOURCE_HPP_
#define EB_PLUGIN_EVENTSOURCE_HPP_

#include "Owned.hpp"

namespace eb_plugin {

class TimingReceiver;

class EventSource : public Owned {
public:
	EventSource(saftbus::Container *container);

	/// @brief The precision of generated timestamps in nanoseconds.
	/// @return The precision of generated timestamps in nanoseconds.
	///
	// @saftbus-export
    virtual uint64_t getResolution() const = 0;

	/// @brief How many bits of external data are included in the ID
	/// @return How many bits of external data are included in the ID
	///
	// @saftbus-export
    virtual uint32_t getEventBits() const = 0;


    /// @brief Should the event source generate events
    /// @return true if the event source generates events
    ///
    // @saftbus-export
    virtual bool getEventEnable() const = 0;
    virtual void setEventEnable(bool val) = 0;

    /// @brief Combined with low EventBits to create generated IDs
    /// @return the event prefix
    ///
    // @saftbus-export
    virtual uint64_t getEventPrefix() const = 0;
    virtual void setEventPrefix(uint64_t val) = 0;

};

}

#endif
