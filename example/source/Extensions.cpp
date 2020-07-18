#include "example/Extensions.h"

#include "spvgentwo/GLSL450Instruction.h"

using namespace spvgentwo;
using namespace ext;

spvgentwo::Module examples::extensions(spvgentwo::IAllocator* _pAllocator, spvgentwo::ILogger* _pLogger)
{
	// create a new spir-v module
	Module module(_pAllocator, spv::Version, spv::AddressingModel::Logical, spv::MemoryModel::VulkanKHR, _pLogger);
   
    module.addCapability(spv::Capability::Shader);

    // use Vulkan Memory Model
    module.addCapability(spv::Capability::VulkanMemoryModelKHR);
    module.addExtension("SPV_KHR_vulkan_memory_model"); // add extension by string


    // Instruction* extId = module.getExtensionInstructionImport("GLSL.std.450");

    // void entryPoint();
    {
        EntryPoint& entry = module.addEntryPoint(spv::ExecutionModel::Fragment, "main");
        entry.addExecutionMode(spv::ExecutionMode::OriginUpperLeft);
        BasicBlock& bb = *entry; // get entry block to this function

        // SPV_AMD_gcn_shader example: https://htmlpreview.github.io/?https://github.com/KhronosGroup/SPIRV-Registry/blob/master/extensions/AMD/SPV_AMD_gcn_shader.html
        {
            module.addExtension("SPV_AMD_gcn_shader"); // adds an OpExtension instruction to the module

            // opcodes taken from extension spec:
            const unsigned int CubeFaceCoordAMD = 2;
            const unsigned int CubeFaceIndexAMD = 1;
            const unsigned int TimeAMD = 3;

            // getExtensionInstructionImport adds extension to the module
            // return value ext can be used with Instruction.opExtInst(resultType, extId, opCode, ...);
            Instruction* extId = module.getExtensionInstructionImport("SPV_AMD_gcn_shader");

            /// CubeFaceCoordAMD example:

            // Result Type must be a 2-component 32-bit floating-point vector.
            Instruction* vec2 = module.type<vector_t<float, 2>>();

            // The operand <P> must be a pointer to a 3-component 32-bit floating-point vector.
            Instruction* P = entry.variable<vector_t<float, 3>>();

            // use extId generated by getExtensionInstructionImport (which adds OpExtInstImport)
            Instruction* faceCoord = bb->opExtInst(vec2, extId, CubeFaceCoordAMD, P);

            /// TimeAMD example:

            //Use of this instruction requires declaration of the Int64 capability.
            //Result Type must be a 64 - bit unsigned integer scalar.
            Instruction* uint64 = module.type<unsigned long int>();

            // The second variant of opExtInst can direclty add the extension to the module by supplying the extension name "GLSL.std.450"
            Instruction* time = bb->opExtInst(uint64, "SPV_AMD_gcn_shader", TimeAMD);
        }

        // SpvGenTwo comes with GLSL extension instructions (GLSL450Intruction derives from Instruction>
        Instruction* vec3 = module.constant(make_vector(1.f, 2.f, 3.f));

        // BasicBlock template function ext<T> adds a new Instruction and casts it to type T
        // this works as long as T does not add any data members and just functionally extends the Instruction class
        Instruction* cross = bb.ext<GLSL>()->opCross(vec3, vec3); // use GLSL.std.450 extension
       
        Instruction* norm = bb.ext<GLSL>()->opNormalize(cross);

        Instruction* ff = bb.ext<GLSL>()->opFaceForward(vec3, norm, cross);

        Instruction* refl = bb.ext<GLSL>()->opReflect(vec3, norm);

        Instruction* eta = bb->opDot(refl, ff);

        Instruction* refr = bb.ext<GLSL>()->opRefract(refl, ff, eta);

        Instruction* len = bb.ext<GLSL>()->opLength(refr);

        Instruction* dist = bb.ext<GLSL>()->opDistance(refl, refr);

        entry->opReturn();
    }

	return module;
}
