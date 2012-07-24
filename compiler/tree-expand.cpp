/*
	Aseba - an event-based framework for distributed robot control
	Copyright (C) 2007--2012:
		Stephane Magnenat <stephane at magnenat dot net>
		(http://stephane.magnenat.net)
		and other contributors, see authors.txt for details

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published
	by the Free Software Foundation, version 3 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "compiler.h"
#include "tree.h"
#include "../utils/FormatableString.h"

#include <cassert>
#include <memory>
#include <iostream>
#include <typeinfo>

namespace Aseba
{
	Node* Node::treeExpand(std::wostream *dump, unsigned int index)
	{
		// recursively walk the tree
		for (NodesVector::iterator it = children.begin(); it != children.end();)
		{
			*(it) = (*it)->treeExpand(dump, index);
			++it;
		}
		return this;
	}


	Node* AssignmentNode::treeExpand(std::wostream *dump, unsigned int index)
	{
		assert(children.size() == 2);

		unsigned lSize = children[0]->getMemorySize();
		unsigned rSize = children[1]->getMemorySize();

		// consistency check
		if (lSize != rSize)
			throw Error(sourcePos, WFormatableString(L"Inconsistent size! Left size: %0, right size: %1").arg(lSize).arg(rSize));

		MemoryVectorNode* leftVector = dynamic_cast<MemoryVectorNode*>(children[0]);
		assert(leftVector);
		leftVector->setWrite(true);
		Node* rightVector = children[1];

		std::auto_ptr<BlockNode> block(new BlockNode(sourcePos));
		// for each dimension
		for (unsigned int i = 0; i < lSize; i++)
		{
			std::auto_ptr<AssignmentNode> assignment(new AssignmentNode(sourcePos));
			assignment->children.push_back(leftVector->treeExpand(dump, i));
			assignment->children.push_back(rightVector->treeExpand(dump, i));
			block->children.push_back(assignment.release());
		}

		delete this;
		return block.release();
	}


	Node* BinaryArithmeticNode::treeExpand(std::wostream *dump, unsigned int index)
	{
		unsigned lSize = children[0]->getMemorySize();
		unsigned rSize = children[1]->getMemorySize();

		// consistency check
		if (lSize != rSize)
			throw Error(sourcePos, WFormatableString(L"Inconsistent size! Left size: %0, right size: %1").arg(lSize).arg(rSize));

		std::auto_ptr<Node> left(children[0]->treeExpand(dump, index));
		std::auto_ptr<Node> right(children[1]->treeExpand(dump, index));
		return new BinaryArithmeticNode(sourcePos, op, left.release(), right.release());
	}


	Node* UnaryArithmeticNode::treeExpand(std::wostream *dump, unsigned int index)
	{
		std::auto_ptr<Node> left(children[0]->treeExpand(dump, index));
		return new UnaryArithmeticNode(sourcePos, op, left.release());
	}


	Node* StaticVectorNode::treeExpand(std::wostream *dump, unsigned int index)
	{
		return new ImmediateNode(sourcePos, getValue(index));
	}


	Node* MemoryVectorNode::treeExpand(std::wostream *dump, unsigned int index)
	{
		assert(index < getMemorySize());

		if (write == true)
		{
			return new StoreNode(sourcePos, getMemoryAddr() + index);
		}
		else
		{
			return new LoadNode(sourcePos, getMemoryAddr() + index);
		}

		return this;
	}


	unsigned Node::getMemorySize() const
	{
		unsigned size = E_NOVAL;
		unsigned new_size = E_NOVAL;

		for (NodesVector::const_iterator it = children.begin(); it != children.end(); ++it)
		{
			new_size = (*it)->getMemorySize();
			if (size == E_NOVAL)
				size = new_size;
			else if (size != new_size)
				throw Error(sourcePos, L"Size mismatch between vectors");
		}

		return size;
	}


	unsigned Node::getMemoryAddr() const
	{
		if (children.size() > 0)
			return children[0]->getMemoryAddr();
		else
			return E_NOVAL;
	}


	void Node::releaseChildren()
	{
		for (NodesVector::iterator it = children.begin(); it != children.end(); ++it)
		{
			*(it) = 0;
		}
	}

	int StaticVectorNode::getLonelyImmediate() const
	{
		assert(getMemorySize() == 1);
		return values[0];
	}

	int StaticVectorNode::getValue(unsigned index) const
	{
		assert(index < getMemorySize());
		return values[index];
	}

	unsigned MemoryVectorNode::getMemoryAddr() const
	{
		assert(children.size() <= 1);

		unsigned shift = 0;

		// index(es) given?
		if (children.size() == 1)
		{
			StaticVectorNode* index = dynamic_cast<StaticVectorNode*>(children[0]);
			if (index)
			{
				shift = index->getValue(0);
			}
			else
				// not know at compile time
				return E_NOVAL;
		}

		return arrayAddr + shift;
	}


	unsigned MemoryVectorNode::getMemorySize() const
	{
		assert(children.size() <= 1);

		if (children.size() == 1)
		{
			StaticVectorNode* index = dynamic_cast<StaticVectorNode*>(children[0]);
			if (index)
			{
				// immediate indexes
				if (index->getMemorySize() == 1)
					// 1 index is given -> 1 dimension
					return 1;
				else
				{
					// 2 indexes are given -> compute the span
					return index->getValue(1) - index->getValue(0) + 1;
				}
			}
			else
				// 1 index, random access
				return 1;
		}
		else
			// full array access
			return arraySize;
	}
}
