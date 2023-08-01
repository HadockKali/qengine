#ifndef QHOOK_UTIL_H
#define QHOOK_UTIL_H

#include <windows.h>

#include <new>
#include<string>
#include <vector>

#pragma region Preprocessor

#ifdef _WIN64

#include "../extern/capstone64/include/capstone/capstone.h"
#pragma comment(lib, "capstone64.lib")

#elif defined(_WIN32)

#include "../extern/capstone32/include/capstone/capstone.h"
#pragma comment(lib, "capstone32.lib")

#endif

namespace qengine {

	namespace qhook {

		typedef struct qhook_detection_t {

			bool is_hook;

			size_t hook_length;

			uintptr_t hook_address;

			std::vector<unsigned char> hook_data;
		};

		struct qhook_util {
			
		public:

			static inline qhook_detection_t* __fastcall analyze_fn_hook_presence( const void* fn_address, const size_t fn_length, LPMODULEINFO info_m, csh cs_handle_) { // return nullptr if no hook detect

				if (!fn_address)
					return nullptr;

				cs_insn* instructions = nullptr;

				auto disasm_count = cs_disasm(cs_handle_, static_cast<const uint8_t*>(fn_address), fn_length, reinterpret_cast<uintptr_t>(fn_address), static_cast<size_t>(fn_length * 16), &instructions);

				if (!disasm_count)
					return nullptr;

				for (size_t i = 0; i < disasm_count; ++i) {
					
					if (instructions[i].id == X86_INS_JMP || instructions[i].id == X86_INS_CALL) { // if immediate check if address is in valid module address space, if regiter check last mov to register
						
						if (instructions[i].detail->x86.operands[0].type == X86_OP_IMM) { // jmp / call immediate address, check if in module address space
							
							const auto& operand_address = instructions[i].detail->x86.operands[0].imm;

							if (operand_address < reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) || operand_address > ( reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) + info_m->SizeOfImage) ) {

								auto* hook_o = new qhook_detection_t{
									true,
									static_cast<size_t>(instructions[i].size),
									instructions[i].address
								};

								for (size_t i = 0; i < instructions[i].size; ++i)
									hook_o->hook_data.push_back(instructions[i].bytes[i]);

								return hook_o;
							}

						}
						else if (instructions[i].detail->x86.operands[0].type = X86_OP_REG) { // jmp / call to register val, check for last write to register
							
							if (!i)
								continue;
							
							for (intptr_t x = i - 1; x > -1; --x) {

								if (instructions[x].id == X86_INS_MOV || instructions[x].id == X86_INS_MOVABS) {

									if (( instructions[x].detail->x86.operands[0].type == X86_OP_REG ) && instructions[x].detail->x86.operands[1].type == X86_OP_IMM) { // immediate mov into register

										if (instructions[x].detail->x86.operands[0].reg == instructions[i].detail->x86.operands[0].reg) {

											const auto& operand_address = instructions[x].detail->x86.operands[1].imm;

											if (operand_address < reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) || operand_address >(reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) + info_m->SizeOfImage)) { // mov reg jmp reg hook detected

												auto* hook_o = new qhook_detection_t{
													true,
													static_cast<size_t>(instructions[i].address + instructions[i].size) - static_cast<size_t>(instructions[x].address),
													instructions[x].address
												};

												for (auto y = instructions[x].address; y < instructions[i].address + instructions[i].size; ++y)
													hook_o->hook_data.push_back(*reinterpret_cast<const uint8_t*>(y));

												return hook_o;
											}
										}
									}
								}
								else if (instructions[x].id == X86_INS_POP) { // check if top of stack popped into our register

									if (instructions[x].detail->x86.operands[0].type == X86_OP_REG && (instructions[x].detail->x86.operands[0].reg == instructions[i].detail->x86.operands[0].reg) ) { // pop value off stack into same register, scan for last push

										if (!x)
											continue;

										for (intptr_t y = x - 1; y > -1; --y) { // scan for push

											if (instructions[y].id == X86_INS_PUSH && instructions[y].detail->x86.operands[0].type == X86_OP_REG) { // register pushed onto stack

												for (intptr_t z = y - 1; z > -1; --z) {

													if ((instructions[z].id == X86_INS_MOV) && instructions[z].detail->x86.operands[0].type == X86_OP_REG && instructions[z].detail->x86.operands[0].reg == instructions[y].detail->x86.operands[0].reg && instructions[z].detail->x86.operands[1].type == X86_OP_IMM) { // mov operation of naddress into register, any deeper recursion in the hook than this and our detection fails

														const auto& operand_address = instructions[z].detail->x86.operands[1].imm;

														if (operand_address < reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) || operand_address >(reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) + info_m->SizeOfImage)) { // mov reg jmp reg hook detected

															auto* hook_o = new qhook_detection_t{
																true,
																static_cast<size_t>(instructions[i].address + instructions[i].size) - static_cast<size_t>(instructions[z].address),
																instructions[z].address
															};

															for (auto w = instructions[y].address; w < instructions[i].address + instructions[i].size; ++w)
																hook_o->hook_data.push_back(*reinterpret_cast<const uint8_t*>(w));

															return hook_o;
														}
													}
												}
											}
										}
									}
								}
							}
						}

					}
					else if(instructions[i].id == X86_INS_RET) { // check last address pushed to stack
						
						if (!i)
							return nullptr;

						for (intptr_t x = i - 1; x > -1; --x) {

							if (instructions[x].id == X86_INS_PUSH && instructions[x].detail->x86.operands[0].type == X86_OP_REG) { // register pushed onto stack, look for mov into register

								if (!x)
									continue;

								for (intptr_t y = x - 1; y > -1; --y) {

									if ((instructions[y].id == X86_INS_MOV || instructions[y].id == X86_INS_MOVABS) && instructions[y].detail->x86.operands[0].type == X86_OP_REG && instructions[y].detail->x86.operands[0].reg == instructions[y].detail->x86.operands[0].reg && instructions[y].detail->x86.operands[1].type == X86_OP_IMM) { // mov operation of naddress into register, any deeper recursion in the hook than this and our detection fails

										const auto& operand_address = instructions[y].detail->x86.operands[1].imm;

										if (operand_address < reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) || operand_address >(reinterpret_cast<uintptr_t>(info_m->lpBaseOfDll) + info_m->SizeOfImage)) { // mov reg jmp reg hook detected

											auto* hook_o = new qhook_detection_t{
												true,
												static_cast<size_t>(instructions[i].address + instructions[i].size) - static_cast<size_t>(instructions[y].address),
												instructions[y].address
											};

											for (auto z = instructions[y].address; z < instructions[i].address + instructions[i].size; ++z)
												hook_o->hook_data.push_back(*reinterpret_cast<const uint8_t*>(z));

											return hook_o;
										}
									}
								}
							}
						}
					}
				}

				if(disasm_count)
					cs_free(instructions, disasm_count);

				return nullptr;
			}

			static inline unsigned long long __fastcall analyze_fn_length(void* fn_address, csh& cs_handle_) {

				if (!fn_address)
					return NULL;

				size_t function_size = static_cast<size_t>(0x0);

				PBYTE address_iterator = reinterpret_cast<PBYTE>(fn_address);

				cs_insn* instructions = nullptr;

				while (*reinterpret_cast<PWORD>(address_iterator) != static_cast<WORD>(0xCCCC)) {

					auto disasm_count = cs_disasm(cs_handle_, reinterpret_cast<uint8_t*>(address_iterator), 16, reinterpret_cast<uintptr_t>(address_iterator), static_cast<size_t>(0x0), &instructions);

					if (!disasm_count)
						break;

					auto insn_length = instructions[0].size;

					if (insn_length == 0x1) {

						cs_insn* sub_instruction = nullptr;

						auto sub_disasm_count = cs_disasm(cs_handle_, reinterpret_cast<uint8_t*>(address_iterator + 1), 16, reinterpret_cast<uintptr_t>(address_iterator + 1), static_cast<size_t>(0x0), &sub_instruction);

						if (sub_disasm_count) {

							if (sub_instruction[0].size == 0x1) {

								if (*reinterpret_cast<PWORD>(address_iterator) == 0xCCCC) {
									cs_free(sub_instruction, sub_disasm_count);
									break;
								}

							}
							cs_free(sub_instruction, sub_disasm_count);
						}
					}

					function_size += insn_length;
					address_iterator += insn_length;

					cs_free(instructions, disasm_count);

				}

				return function_size;
			}
		
		};
	}
}

#endif