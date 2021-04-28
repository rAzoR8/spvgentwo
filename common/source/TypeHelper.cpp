#include "common/TypeHelper.h"

#include "spvgentwo/Type.h"
#include "spvgentwo/String.h"
#include "spvgentwo/Instruction.h"

namespace
{
	void getTypeName(spvgentwo::sgt_size_t _offset, unsigned int _indent, const spvgentwo::Type& _type, spvgentwo::String& _outName, const spvgentwo::Instruction* _pOpTypeInstr)
	{
		using namespace spvgentwo;

		Instruction::Iterator op = nullptr;

		if (_pOpTypeInstr != nullptr)
		{
			if (_pOpTypeInstr->isType())
			{
				op = _pOpTypeInstr->getFirstActualOperand();
			}
			else if (_pOpTypeInstr->hasResultType())
			{
				op = _pOpTypeInstr->getResultTypeInstr()->getFirstActualOperand();
			}
		}

		auto instrName = [](const spvgentwo::Instruction* _instr) -> const char*
		{
			const char* name = nullptr;

			if (_instr != nullptr)
			{
				if (name = _instr->getName(); name == nullptr && _instr->hasResultType())
				{
					if (const Instruction* t = _instr->getResultTypeInstr(); t != nullptr)
					{
						name = t->getName();
					}
				}
			}

			return name;
		};

		// string
		auto insert = [&](const char* str)
		{
			auto prev = _outName.size();
			_outName.insert(_offset, str);
			auto len = _outName.size() - prev;
			_offset += len;
		};

		// type
		auto insertSubType = [&_outName, &_offset, _indent](const Type& _t, const spvgentwo::Instruction* _pInstr)
		{
			auto prev = _outName.size();
			getTypeName(_offset,_indent + 1u, _t, _outName, _pInstr);
			auto len = _outName.size() - prev;
			_offset += len;
		};

		auto indent = [&insert](unsigned int indent)
		{
			for(auto i = 0u; i < indent; ++i)
			{
				insert("\t");
			}
		};

		if (_type.isStruct())
		{
			indent(_indent);
			insert("struct ");

			if (const char* name = instrName(_pOpTypeInstr); name != nullptr)
			{
				insert(name);
			}

			insert(" {\n");

			for (const Type& t : _type) 
			{
				indent(_indent + 1);
				insertSubType(t, op != nullptr ? op->getInstruction() : nullptr);
				if (t.isStruct() == false)
				{
					insert(";\n");
				}

				++op;
			}
			
			indent(_indent);
			insert("};\n");
		}
		else if (_type.isFunction())
		{
			indent(_indent);
			auto it = _type.begin();

			// return type
			getTypeName(_offset, 0u, *it, _outName, op != nullptr ? op->getInstruction() : nullptr);

			insert(" "); // func name
			if (const char* name = instrName(_pOpTypeInstr); name != nullptr)
			{
				insert(name);
			}
			else
			{
				insert("UNNAMED_FUNCTION");
			}

			// parameters
			insert("(");

			while (++it != _type.end())
			{
				++op;

				// param name
				if (const char* name = instrName(op != nullptr ? op->getInstruction() : nullptr); name != nullptr)
				{
					insert(name);
				}
				else // parameter type
				{
					getTypeName(_offset, 0u, *it, _outName, op != nullptr ? op->getInstruction() : nullptr);
				}

				if (it.next() != nullptr)
				{
					insert(", ");
				}
			}

			insert(")");
		}
		else if (_type.isPointer())
		{
			indent(_indent);
			if (const char* name = instrName(_pOpTypeInstr); name != nullptr)
			{
				insert(name);
			}
			else // parameter type
			{
				insertSubType(_type.front(), _pOpTypeInstr); // or pass subtype instr?
			}

			insert("*");
		}
		else if(const char* name = _type.getString(); name != nullptr)
		{
			_outName.insert(_offset, name);
		}
	}
} // anon

void spvgentwo::TypeHelper::getTypeName(const Type& _type, String& _outName, const Instruction* _pOpTypeInstr)
{
	::getTypeName(0u, 0u, _type, _outName, _pOpTypeInstr);
}