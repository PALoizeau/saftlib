/** Copyright (C) 2011-2016, 2021-2022 GSI Helmholtz Centre for Heavy Ion Research GmbH 
 *
 *  @author Wesley W. Terpstra <w.terpstra@gsi.de>
 *          Michael Reese <m.reese@gsi.de>
 *
 *******************************************************************************
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************
 */

#include "EmbeddedCPUActionSink.hpp"
#include "ECA.hpp"
#include "eca_queue_regs.h"
#include "eca_flags.h"

#include "Condition.hpp"
#include "EmbeddedCPUCondition.hpp"
#include "EmbeddedCPUCondition_Service.hpp"


#include <cassert>
#include <sstream>
#include <memory>

namespace saftlib {

EmbeddedCPUActionSink::EmbeddedCPUActionSink(ECA &eca
                                  , const std::string &obj_path
                                  , const std::string &name
                                  , unsigned channel
                                  , saftbus::Container *container)
	: ActionSink(eca, obj_path, name, channel, 0, container)
{
}



std::string EmbeddedCPUActionSink::NewCondition(bool active, uint64_t id, uint64_t mask, int64_t offset, uint32_t tag)
{
	return NewConditionHelper<EmbeddedCPUCondition>(active, id, mask, offset, tag, container);
}

}