#pragma once

#include "List.h"
#include "Operand.h"
#include "SpvDefines.h"
#include "Flag.h"

namespace spvgentwo
{
	// forward delcs
	class Type;

	static constexpr spv::Id InvalidId = 0xFFFFFFFF;

	class Instruction : public List<Operand>
	{
	public:
		using Iterator = EntryIterator<Operand>;

		template <class ...Args>
		Instruction(IAllocator* _pAllocator, const spv::Op _op = spv::Op::OpNop, Args&& ... _args);
		template <class ...Args>
		Instruction(BasicBlock* _pBasicBlock, const spv::Op _op = spv::Op::OpNop, Args&& ... _args);

		~Instruction();

		BasicBlock* getBasicBlock() { return m_pBasicBlock; }
		const BasicBlock* getBasicBlock() const { return m_pBasicBlock; }

		// manual instruction construction:
		void setOperation(const spv::Op _op) { m_Operation = _op; };
		spv::Op getOperation() const { return m_Operation; }
		template<class ...Args>
		Operand& addOperand(Args&& ... _operand) { return emplace_back(std::forward<Args>(_operand)...); }

		spv::Id getId() const { return m_ResultId; }
		const Instruction* getType() const { return m_pType; }
		void setType(const Instruction* _pType);

		bool isTypeOp() const;

		void reset();

		// get number of 32 bit words used by this instruction
		unsigned int getWordCount() const;
		unsigned int getOpCode() const;

		void write(IWriter* _pWriter) const;

		// creates literals
		template <class T, class ...Args>
		void makeOp(T first, Args ... _args);

		void appendLiterals(const char* _pStr);

		template <class T, class ...Args>
		void appendLiterals(T first, Args ... _args);

		// instruction generators:
		// all instructions generating a result id return a pointer to this instruction for reference (passing to other instruction operand)
		void opCapability(const spv::Capability _capability);

		void opMemoryModel(const spv::AddressingModel _addressModel, const spv::MemoryModel _memoryModel);

		void opExtension(const char* _pExtName);

		Instruction* opExtInstrImport(const char* _pExtName);

		Instruction* opLabel();

		Instruction* opFunction(const Flag<spv::FunctionControlMask> _functionControl, const Instruction* _pResultType, const Instruction* _pFuncType);

		Instruction* opFunctionParameter(const Instruction* _pType);

		void opFunctionEnd();

	private:
		spv::Op m_Operation = spv::Op::OpNop;
		const Instruction* m_pType = nullptr; // result type

		BasicBlock* m_pBasicBlock = nullptr;
		spv::Id m_ResultId = InvalidId;		
	};

	// free helper function
	void writeInstructions(IWriter* _pWriter, const List<Instruction>& _instructions);

	template<class ...Args>
	inline Instruction::Instruction(IAllocator* _pAllocator, const spv::Op _op, Args&& ..._args) :
		m_pBasicBlock(nullptr), List(_pAllocator)
	{
		makeOp(_op, std::forward<Args>(_args)...);
	}

	template<class ...Args>
	inline Instruction::Instruction(BasicBlock* _pBasicBlock, const spv::Op _op, Args&& ..._args) :
		m_pBasicBlock(_pBasicBlock), List(_pBasicBlock->getAllocator())
	{
		makeOp(_op, std::forward<Args>(_args)...);
	}
	
	template<class T, class ...Args>
	inline void Instruction::makeOp(T _first, Args ..._args)
	{
		if constexpr (is_same_base_type_v<T, spv::Op>)
		{
			m_Operation = _first;
		} 
		else if constexpr (is_same_base_type_v<T, Instruction*>)
		{
			if (_first->isTypeOp() && m_pType == nullptr)
			{
				setType(_first);
			}
			else
			{
				addOperand(_first);
			}
		}
		else if constexpr(is_same_base_type_v<T, BasicBlock*>)
		{
			addOperand(_first);
		}
		else if constexpr (sizeof(T) == sizeof(unsigned int)) // bitcast to 32 bit literal
		{
			addOperand(*reinterpret_cast<const unsigned int*>(&_first));
		}
		else
		{
			appendLiterals(_first);
		}

		if constexpr (sizeof...(_args) > 0u)
		{
			makeOp(_args...);
		}
	}

	template<class T, class ...Args>
	inline void Instruction::appendLiterals(T first, Args ..._args)
	{
		constexpr auto bytes = sizeof(T);
		constexpr auto words = wordCount(bytes) - (bytes % sizeof(unsigned int) == 0 ? 0 : 1);
		
		for (auto w = 0u; w < words; ++w)
		{
			addOperand(reinterpret_cast<const unsigned int*>(&first)[w]);
		}
		
		if constexpr (bytes % sizeof(unsigned int) != 0)
		{
			unsigned int lastWord{ 0u };
			const auto offset = words * sizeof(unsigned int);
			for (auto b = offset; b < bytes; ++b)
			{
				reinterpret_cast<char*>(&lastWord)[b - offset] = reinterpret_cast<const char*>(&first)[b];
			}
			addOperand(lastWord);
		}		

		if constexpr (sizeof...(_args) > 0u)
		{
			appendLiterals(_args...);
		}
	}

	inline void spvgentwo::Instruction::appendLiterals(const char* _pStr)
	{
		unsigned int word{ 0u };
		unsigned int l = 0u;
		char c = 0u;

		do
		{
			c = _pStr[l];
			const auto mod = l++ % sizeof(unsigned int);
			reinterpret_cast<char*>(&word)[mod] = c;

			if (c == 0 || mod == 3)
			{
				addOperand(word);
				word = 0u;
			}

		} while (c != 0);
	}
} // !spvgentwo