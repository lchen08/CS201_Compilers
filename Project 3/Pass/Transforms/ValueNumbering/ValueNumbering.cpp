#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "ValueNumbering"

using namespace llvm;

namespace {
    struct ValueNumbering : public FunctionPass {
        struct Block {
            string name;
            vector<string> liveOut; 
            vector<string> varKill;
            vector<string> ueVar;
            vector<string> successors;
        };

        string func_name = "test";
        static char ID;
        queue<Block*> worklist;
        vector<Block*> allBlocks;
        int blockNum = 1;
        unordered_map<string, vector<string>> predecessorMap;
        unordered_map<string, string> varMap;
        ValueNumbering() : FunctionPass(ID) {}

        //gives the equivalent variable to a LLVM tmp variable
        string getEquivVar(string var) {
            if(varMap.find(var) != varMap.end()) {
                return varMap.find(var)->second;
            }
            else {
                return var;
            }
        }

        //adds to block's list of UEVars as long as not in varKill
        void addUEVar(Block* block, string op) {
            if (op != "") {
                vector<string>::iterator killIt = find(block->varKill.begin(), block->varKill.end(), op);
                vector<string>::iterator addIt = find(block->ueVar.begin(), block->ueVar.end(), op);

                if (killIt == block->varKill.end() && addIt == block->ueVar.end()) {
                    block->ueVar.push_back(op);
                }
            }
        }

        //adds to block's list of VarKills
        void addVarKill(Block* block, string op) {
            vector<string>::iterator it = find(block->varKill.begin(), 
                block->varKill.end(), op);
            
            if (it == block->varKill.end()) {
                block->varKill.push_back(op);
            }
        }

        //performs vector - vector
        vector<string> removeVars(vector<string> removeFrom, vector<string> removeTo) {
            for (string item : removeTo) {
                vector<string>::iterator it = find(removeFrom.begin(), removeFrom.end(), item);
                if(it != removeFrom.end()) {
                    removeFrom.erase(it);
                }
            }
            return removeFrom;
        }

        //performs vector + vector, with no repeats
        vector<string> addVars(vector<string> addTo, vector<string> toAdd) {
            for(string item : toAdd) {
                vector<string>::iterator it = find(addTo.begin(), addTo.end(), item);
                if(it == addTo.end()) {
                    addTo.push_back(item);
                }
            }
            return addTo;
        }

        //checks if live out is changed using size (live out only grows)
        bool liveOutIsChanged(vector<string> originalVec, vector<string> newVec) {
            return originalVec.size() != newVec.size();
        }

        //finds a block with a given name
        Block* findBlock(string name) {
            for (int i = 0; i <allBlocks.size(); i++) {
                if (allBlocks[i]->name == name) {
                    return allBlocks[i];
                }
            }
            errs() << "Error: Block not found in findBlock(): " << name << "\n";
            assert(false);
            return nullptr;
        }

        //adds predecessors to worklist if not already in worklist
        void addPredecessorsToWorklist(string blockName) {
            Block* block = findBlock(blockName);

            if(predecessorMap.find(blockName) != predecessorMap.end()) {
                vector<string> predecessors = predecessorMap.find(blockName)->second;
                for (string predecessorName : predecessors) {
                    queue<Block*> newWorkList;
                    bool found = false;
                    while (!worklist.empty()) {
                        Block* worklistBlock = worklist.front();
                        worklist.pop();
                        newWorkList.push(worklistBlock);
                        if (worklistBlock->name == predecessorName) {
                            found = true;
                        }
                    }
                    if(!found) {
                        Block* predBlock = findBlock(predecessorName);
                        if (predBlock != nullptr) {
                            newWorkList.push(predBlock);
                        }
                        else {
                            //shouldn't occur - name given is wrong
                            errs() << "Trying to push a null block to worklist for block :" 
                                << predecessorName << ". This is the predecessor of :" << blockName << "\n";
                        }
                    }
                    worklist = newWorkList;
                }
            }
        }

        void processWorklist() {
            while (!worklist.empty()) {
                Block* currentBlock = worklist.front();
                worklist.pop();
                vector<string> newLiveOut;
                vector<Block*> successorBlocks;
                for (string successorName : currentBlock->successors) {
                    Block* successor = findBlock(successorName);
                    successorBlocks.push_back(successor);
                    vector<string> partialLiveOut = addVars(removeVars(successor->liveOut, successor->varKill),successor->ueVar);
                    newLiveOut = addVars(newLiveOut, partialLiveOut);
                }
                if(liveOutIsChanged(currentBlock->liveOut, newLiveOut)) {
                    currentBlock->liveOut = newLiveOut;
                    addPredecessorsToWorklist(currentBlock->name);
                }
                
            }
        }

        bool runOnFunction(Function &F) override {
            // errs() << "ValueNumbering: " << F.getName() << "\n";

            // if (F.getName() != func_name) return false;

            for (auto& basic_block : F)
            {
                Block* newBlock = new Block();
                newBlock->name = basic_block.getName();
                errs() << "\n\nThis is start of a basic block " << newBlock->name << "\n";

                for (auto& inst : basic_block)//inside basic block
                {
                    errs() << inst << "\n";
                    string instName = inst.getName();

                    if(inst.getOpcode() == Instruction::Load){
                        errs() << "This is Load"<<"\n";
                        //Do not need to process this instruction (not needed for Liveness Analysis)
                        // string loadFromOp = removeDotAddr(inst.getOperand(0)->getName());
                        string loadFromOp = inst.getOperand(0)->getName();
                        varMap[instName] = loadFromOp;
                    }

                    if(inst.getOpcode() == Instruction::Store){
                        errs() << "This is Store"<<"\n";

                        string tmpVarBaseName = "tmp";
                        string storeTo = inst.getOperand(1)->getName();
                        string storeFrom = inst.getOperand(0)->getName();

                        //if not constant and scenario tmp = x where tmp represents a var
                        if (storeFrom != "" && (storeFrom.find(tmpVarBaseName) != string::npos)) {
                            addUEVar(newBlock, getEquivVar(storeFrom));
                        }

                        addVarKill(newBlock, getEquivVar(storeTo));
                    }

                    if (inst.isBinaryOp())
                    {
                        
                        errs() << "This is Binary Op"<<"\n";
                        
                        addUEVar(newBlock, getEquivVar(inst.getOperand(0)->getName()));
                        addUEVar(newBlock, getEquivVar(inst.getOperand(1)->getName()));
                    } // end if
                } // end for inst
                
                if (BranchInst *bi = dyn_cast<BranchInst>(basic_block.getTerminator())) {
                    int numSuccessors = bi->getNumSuccessors();
                    int start, end;
                    if (numSuccessors == 2) {
                        start = 1;
                        end = 2;
                    }
                    else {
                        start = 0;
                        end = 0;
                    }

                    for (int successor = start; successor <= end; successor++) {
                        string successorName = bi->getOperand(successor)->getName();
                        // errs() << "Successor Name: " << successorName << "\n";
                        newBlock->successors.push_back(successorName);

                       //found in hash map 
                        if(predecessorMap.find(successorName) != predecessorMap.end()) {
                            predecessorMap.find(successorName)->second.push_back(basic_block.getName());
                        }

                        //not found in hashmap
                        else {
                            vector<string> predecessorList;

                            predecessorList.push_back(basic_block.getName());
                            predecessorMap[successorName] = predecessorList;
                        }
                    }
                }
                worklist.push(newBlock);
                allBlocks.push_back(newBlock);
            } // end for block

            processWorklist();

            errs() << "\n\n";
            for (Block* block : allBlocks) {
                errs() << "\nBlock name: " << block->name << "";
                errs() << "\nLiveOut\n";
                for (string liveItem : block->liveOut) {
                    errs() << liveItem << " ";
                }
                errs() << "\nUEVar\n";
                for (string ueItem: block->ueVar) {
                    errs() << ueItem << " ";
                }
                errs() << "\nVarKill\n";
                for (string killItem: block->varKill) {
                    errs() << killItem << " ";
                }
                errs() << "\n";
            }
            return false;
        } // end runOnFunction
    }; // end of struct ValueNumbering
}  // end of anonymous namespace

char ValueNumbering::ID = 0;
static RegisterPass<ValueNumbering> X("ValueNumbering", "ValueNumbering Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
