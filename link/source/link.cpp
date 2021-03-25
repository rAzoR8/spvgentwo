#include "spvgentwo/Module.h"
#include "spvgentwo/Grammar.h"
#include "spvgentwo/Templates.h"

#include "common/BinaryFileReader.h"
#include "common/BinaryFileWriter.h"
#include "common/ConsoleLogger.h"
#include "common/HeapList.h"
#include "common/HeapString.h"
#include "common/ModulePrinter.h"
#include "common/LinkerHelper.h"

#include <cstring> // strcmp
#include <cstdio> // printf, note windows console, and others too, don't print SPIR-V's UTF-8 strings properly
#include <cstdlib> // strtoul

using namespace spvgentwo;
using namespace ModulePrinter;

auto g_printer = ModuleSimpleFuncPrinter([](const char* _pStr) { printf("%s", _pStr); });
ConsoleLogger g_logger;
HeapAllocator g_alloc;
const Grammar g_gram(&g_alloc);

// import/export target
struct Target
{
	Instruction* instr = nullptr;
	const char* name = nullptr;
	spv::LinkageType type = spv::LinkageType::Max;
	bool exportAllReferencedVars = false; // for instr == OpFunction export
};

int patch(Module& _module, const HeapList<Target>& _targets, const char* _out)
{
	_module.checkAddCapability(spv::Capability::Linkage);

	for (auto& t : _targets)
	{
		if (*t.instr == spv::Op::OpFunction && t.instr->getFunction()->isEntryPoint() == false)
		{
			Function* func = t.instr->getFunction();

			if (t.type == spv::LinkageType::Import)
			{
				if (func->empty() == false)
				{
					g_logger.logInfo("Functions marked for IMPORT must not contain any basic blocks, removing %u blocks", func->size());
					LinkerHelper::removeFunctionBody(*func);
				}
			}
			else if (t.type == spv::LinkageType::Export && t.exportAllReferencedVars)
			{
				HeapList<Operand> vars;
				collectReferencedVariables(*func, vars, GlobalInterfaceVersion::SpirV14_x, &g_alloc);

				for (const Operand& op : vars)
				{
					if (Instruction* var = op.getInstruction(); var != nullptr)
					{
						Instruction* instr = _module.addDecorationInstr();

						if (const char* name = var->getName(); name != nullptr)
						{
							instr->opDecorate(var, spv::Decoration::LinkageAttributes, name, t.type);
							g_printer << "Added "; printInstruction(*instr, g_gram, g_printer); g_printer << "\n";
						}
						else
						{
							g_logger.logError("OpVariable Id %u has no OpName for exporting");
							return -1;
						}
					}
				}
			}
		}

		if (*t.instr != spv::Op::OpFunction && *t.instr != spv::Op::OpVariable)
		{
			g_logger.logError("Only OpFunctions which are not EntryPoints and global OpVariables can be imported/exported [%s]", g_gram.getInfo(static_cast<unsigned int>(t.instr->getOperation())));
			return -1;
		}

		Instruction* instr = _module.addDecorationInstr();
		instr->opDecorate(t.instr, spv::Decoration::LinkageAttributes, t.name, t.type);
		g_printer << "Added "; printInstruction(*instr, g_gram, g_printer); g_printer << "\n";
	}

	BinaryFileWriter writer(_out);
	if (writer.isOpen() == false)
	{
		g_logger.logError("Failed to open %s", _out);
		return -1;
	}

	_module.write(&writer);
	return 0;
}

int main(int argc, char* argv[])
{
	const char* out = nullptr;
	const char* patchspv = nullptr;
	
	HeapList<Target> targets;
	HeapList<Module> libs;

	Module patchModule(&g_alloc, spv::Version, &g_logger);

	int i = 1u;
	auto addTarget = [&](spv::LinkageType type)
	{
		Instruction* instr = nullptr;
		const char* target = argv[++i];
		if (auto id = strtoul(target, nullptr, 10); id != 0 && id != sgt_uint32_max)
		{
			instr = patchModule.getInstructionById(spv::Id{ id });
		}
		else
		{
			instr = patchModule.getInstructionByName(target);
		}

		if (instr == nullptr)
		{
			g_logger.logError("Could not find %s in %s", target, patchspv);
			return;
		}

		if (const char* name = argv[++i]; name != nullptr)
		{
			bool exportVars = false;
			if (i + 1 < argc && (strcmp(argv[i + 1], "--exportvars") == 0))
			{
				++i;
				exportVars = true;
			}
			targets.emplace_back(instr, name, type, exportVars);
		}
	};

	for (; i < argc; ++i)
	{
		const char* arg = argv[i];
		if (i + 1 < argc && strcmp(arg, "--patchspv") == 0)
		{
			patchspv = argv[++i];
			BinaryFileReader reader(patchspv);
			if (reader.isOpen() == false)
			{
				g_logger.logError("Failed to open %s", patchspv);
				return -1;
			}
			else if (patchModule.readAndInit(&reader, g_gram) == false)
			{
				return -1;
			}
		}
		else if (i + 1 < argc && strcmp(arg, "--l") == 0)
		{
			const char* file = argv[++i];
			BinaryFileReader reader(file);
			if (reader.isOpen() == false)
			{
				g_logger.logError("Failed to open %s", file);
				return -1;
			}
			else if (libs.emplace_back(&g_alloc, spv::Version, &g_logger).readAndInit(&reader, g_gram) == false)
			{
				return -1;
			}
		}
		else if (i + 1 < argc && strcmp(arg, "--out") == 0)
		{
			out = argv[++i];
		}
		else if (i + 1 < argc && strcmp(arg, "--export") == 0)
		{
			addTarget(spv::LinkageType::Export);
		}
		else if (i + 1 < argc && strcmp(arg, "--import") == 0)
		{
			addTarget(spv::LinkageType::Import);
		}
	}

	if (targets.empty() == false)
	{
		return patch(patchModule, targets, out != nullptr ? out : patchspv);
	}

	return 0;
}