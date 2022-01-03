#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include <string>
#include <unordered_map>
#include <sstream>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "ValueNumbering"

using namespace llvm;

namespace {
struct ValueNumbering : public FunctionPass {
    string func_name = "test";
    static char ID;
    ValueNumbering() : FunctionPass(ID) {}
    int currentHash = 1;
    string addrTag = ".addr";

    int getHashValue(unordered_map<string, int> hashmap, string key) {
        if(hashmap.find(key) != hashmap.end()) {
            return hashmap.find(key)->second;
        }
        else {
            // errs() << "Could not find key in hashmap: " << key;
            return -1;
        }
    }

    string removeDotAddr(string word) {
        size_t dotPos;
        if ((dotPos = word.find_first_of(".")) != string::npos) {
            return word.substr(0, dotPos);
        }
        return word;
    }

    string getStrOper(Value* val) {
        string operStr;
        raw_string_ostream rs(operStr);
        rs << *val;
        // errs() << "stringstream " << operStr << "\n";
        return operStr;
    }

    bool runOnFunction(Function &F) override {
	    
        errs() << "ValueNumbering: " << F.getName() << "\n";
        // if (F.getName() != func_name) return false;

        for (auto& basic_block : F)
        {
            unordered_map<string, int> hashmap;
            for (auto& inst : basic_block) //inside basic block
            {
                errs() << inst << "\n";
                string instName = inst.getName();

                if(inst.getOpcode() == Instruction::Load){
                    errs() << "This is Load"<<"\n";
                    
                    //load %var should be new, so add to hashmap with value it is pointing to
                    string loadFromOp = removeDotAddr(inst.getOperand(0)->getName());
                    // errs() << "Load From Op: " << loadFromOp << "\n";
                    int value = getHashValue(hashmap, loadFromOp);
                    if (value == -1) {
                        value = currentHash++;
                    }
                    hashmap[instName] = value;
                }

                if(inst.getOpcode() == Instruction::Store){
                    errs() << "This is Store"<<"\n";

                    string storeFrom = inst.getOperand(0)->getName();
                    //if constant, has no name
                    if (storeFrom == "") {
                        storeFrom= getStrOper(inst.getOperand(0));
                    }

                    //storeTo is always a variable (never a constant)
                    string storeTo = removeDotAddr(inst.getOperand(1)->getName());

                    int value = getHashValue(hashmap, storeFrom);
                    if (value == -1) {
                        value = currentHash++;
                    }
                    hashmap[storeTo] = value;
                    hashmap[storeFrom] = value;
                }

                if (inst.isBinaryOp())
                {
                    string opcodeString;
                    string opString;
                    string op1, op2;

                    // errs() << "Op Code:" << inst.getOpcodeName()<<"\n";
                    if(inst.getOpcode() == Instruction::Add){
                        errs() << "This is Addition"<<"\n";
                        opcodeString = "+";
                    }
                    if(inst.getOpcode() == Instruction::Sub){
                        errs() << "This is Subtraction"<<"\n";
                        opcodeString = "-";
                    }
                    if(inst.getOpcode() == Instruction::Mul){
                        errs() << "This is Multiplication"<<"\n";
                        opcodeString = "*";
                    }
                    if(inst.getOpcode() == Instruction::UDiv || inst.getOpcode() == Instruction::SDiv){
                        errs() << "This is Division"<<"\n";
                        opcodeString = "/";
                    }

                    //chedk if either operands are constants (constants have no names)
                    op1 = removeDotAddr(inst.getOperand(0)->getName());
                    if (op1 == "") {
                        op1 = getStrOper(inst.getOperand(0));
                    }
                    
                    op2 = removeDotAddr(inst.getOperand(1)->getName());
                    if (op2 == "") {
                        op2 = getStrOper(inst.getOperand(1));
                    }
                    
                    //check values in hashmap already
                    int value1 = getHashValue(hashmap, op1);
                    int value2 = getHashValue(hashmap, op2);
                    //not  already in hash map, add to it
                    if(value1 == -1) {
                        value1 = currentHash;
                        hashmap[op1] = currentHash++;
                    }
                    if(value2 == -1) {
                        value2 = currentHash;
                        hashmap[op2] = currentHash++;
                    }

                    opString = to_string(value1) + opcodeString + to_string(value2);
                    int opStrValue = getHashValue(hashmap, opString);
                    errs() << "OpString: " << opString << "\n";

                    //not already in hash map, add to it
                    if(opStrValue == -1) {
                        hashmap[opString] = currentHash;
                        hashmap[instName] = currentHash++;
                    }
                    else {
                        hashmap[instName] = opStrValue;
                        errs() << "\n\tRedundancy found: " << inst << "\n\n";
                    }
                } // end if
            

            } // end for inst
            errs() << "Keys\n";
            for (const auto& kv : hashmap) {
                errs() << kv.first << " " << kv.second <<  "\n";
            }
            errs() << "\n";
        } // end for block
        return false;
    } // end runOnFunction
}; // end of struct ValueNumbering
}  // end of anonymous namespace

char ValueNumbering::ID = 0;
static RegisterPass<ValueNumbering> X("ValueNumbering", "ValueNumbering Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
