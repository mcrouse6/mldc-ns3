/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 Georgia Tech Research Corporation, INRIA
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
 * Authors: George Riley <riley@ece.gatech.edu>
 *          Gustavo Carneiro <gjcarneiro@gmail.com>,
 *          Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#ifndef SIMPLE_REF_COUNT_H
#define SIMPLE_REF_COUNT_H

#include "empty.h"
#include "default-deleter.h"
#include <stdint.h>

namespace ns3 {

/**
 * \brief A template-based reference counting class
 *
 * This template can be used to give reference-counting powers
 * to a class. This template does not require this class to
 * have a virtual destructor or no parent class.
 * 
 * Note: if you are moving to this template from the RefCountBase class,
 * you need to be careful to mark appropriately your destructor virtual
 * if needed. i.e., if your class has subclasses, _do_ mark your destructor
 * virtual.
 *
 *
 * This template takes 3 arguments but only the first argument is
 * mandatory:
 *    - T: the typename of the subclass which derives from this template
 *      class. Yes, this is weird but it's a common C++ template pattern
 *      whose name is CRTP (Curiously Recursive Template Pattern)
 *    - PARENT: the typename of the parent of this template. By default,
 *      this typename is 'ns3::empty' which is an empty class: compilers
 *      which implement the RBCO optimization (empty base class optimization)
 *      will make this a no-op
 *    - DELETER: the typename of a class which implements a public static 
 *      method named 'Delete'. This method will be called whenever the
 *      SimpleRefCount template detects that no references to the object
 *      it manages exist anymore.
 *
 * Interesting users of this class include ns3::Object as well as ns3::Packet.
 */
template <typename T, typename PARENT = empty, typename DELETER = DefaultDeleter<T> >
class SimpleRefCount : public PARENT
{
public:
  SimpleRefCount ()
    : m_count (1)
  {}
  SimpleRefCount (const SimpleRefCount &o)
    : m_count (1)
  {}
  SimpleRefCount &operator = (const SimpleRefCount &o)
  {
    return *this;
  }
  /**
   * Increment the reference count. This method should not be called
   * by user code. SimpleRefCount instances are expected to be used in
   * conjunction with the Ptr template which would make calling Ref
   * unnecessary and dangerous.
   */
  inline void Ref (void) const
  {
    m_count++;
  }
  /**
   * Decrement the reference count. This method should not be called
   * by user code. SimpleRefCount instances are expected to be used in 
   * conjunction with the Ptr template which would make calling Ref
   * unnecessary and dangerous.
   */
  inline void Unref (void) const
  {
    m_count--;
    if (m_count == 0)
      {
        DELETER::Delete (static_cast<T*> (const_cast<SimpleRefCount *> (this)));
      }
  }

  /**
   * Get the reference count of the object.  
   * Normally not needed; for language bindings.
   */
  inline uint32_t GetReferenceCount (void) const
  {
    return m_count;
  }

  static void Cleanup (void) {}
private:
  // Note we make this mutable so that the const methods can still
  // change it.
  mutable uint32_t m_count;
};

} // namespace ns3

#endif /* SIMPLE_REF_COUNT_H */
