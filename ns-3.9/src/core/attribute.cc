/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "attribute.h"
#include "log.h"
#include "fatal-error.h"
#include <sstream>

NS_LOG_COMPONENT_DEFINE ("AttributeValue");

namespace ns3 {

AttributeValue::AttributeValue ()
{}
AttributeValue::~AttributeValue ()
{}


AttributeAccessor::AttributeAccessor ()
{}
AttributeAccessor::~AttributeAccessor ()
{}

AttributeChecker::AttributeChecker ()
{}
AttributeChecker::~AttributeChecker ()
{}

EmptyAttributeValue::EmptyAttributeValue ()
{}
Ptr<AttributeValue> 
EmptyAttributeValue::Copy (void) const
{
  return Create<EmptyAttributeValue> ();
}
std::string 
EmptyAttributeValue::SerializeToString (Ptr<const AttributeChecker> checker) const
{
  return "";
}
bool 
EmptyAttributeValue::DeserializeFromString (std::string value, Ptr<const AttributeChecker> checker)
{
  return true;
}


} // namespace ns3
