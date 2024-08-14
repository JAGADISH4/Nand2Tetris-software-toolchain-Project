#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <bitset>
#include <algorithm>

#define ADDRESS_LEN 15
#define VAR_ADDRESS 16

enum CommandType {
    A_COMMAND,
    C_COMMAND,
    L_COMMAND,
    INVALID_COMMAND
};

class SymbolTable {
private:
    std::unordered_map<std::string, int> table;
    int nextVariableAddress = VAR_ADDRESS;

public:
    SymbolTable() {
        // Predefined symbols
        table["SP"] = 0;
        table["LCL"] = 1;
        table["ARG"] = 2;
        table["THIS"] = 3;
        table["THAT"] = 4;
        for (int i = 0; i < 16; ++i) {
            table["R" + std::to_string(i)] = i;
        }
        table["SCREEN"] = 16384;
        table["KBD"] = 24576;
    }

    bool contains(const std::string& symbol) {
        return table.find(symbol) != table.end();
    }

    int getAddress(const std::string& symbol) {
        return table[symbol];
    }

    void addEntry(const std::string& symbol, int address) {
        table[symbol] = address;
    }

    int addVariable(const std::string& symbol) {
        if (!contains(symbol)) {
            table[symbol] = nextVariableAddress++;
        }
        return table[symbol];
    }
};

class Parser {
private:
    std::ifstream file;
    std::string currentCommand;
    CommandType commandType;
    std::string symbol;
    std::string dest;
    std::string comp;
    std::string jump;

public:
    Parser(const std::string& filename) {
        file.open(filename);
    }

    ~Parser() {
        file.close();
    }

    bool hasMoreCommands() {
        return !file.eof();
    }

    void advance() {
        if (hasMoreCommands()) {
            std::getline(file, currentCommand);
            // Remove whitespace and comments
            currentCommand = currentCommand.substr(0, currentCommand.find("//"));
            currentCommand.erase(std::remove_if(currentCommand.begin(), currentCommand.end(), ::isspace), currentCommand.end());
            if (!currentCommand.empty()) {
                parseCommand();
            }
        }
    }

    CommandType getCommandType() {
        return commandType;
    }

    std::string getSymbol() {
        return symbol;
    }

    std::string getDest() {
        return dest;
    }

    std::string getComp() {
        return comp;
    }

    std::string getJump() {
        return jump;
    }

private:
    void parseCommand() {
        if (currentCommand[0] == '@') {
            commandType = A_COMMAND;
            symbol = currentCommand.substr(1);
        } else if (currentCommand[0] == '(' && currentCommand.back() == ')') {
            commandType = L_COMMAND;
            symbol = currentCommand.substr(1, currentCommand.size() - 2);
        } else if (currentCommand.find('=') != std::string::npos || currentCommand.find(';') != std::string::npos) {
            commandType = C_COMMAND;
            parseCCommand();
        } else {
            commandType = INVALID_COMMAND;
        }
    }

    void parseCCommand() {
        auto eqPos = currentCommand.find('=');
        auto semiPos = currentCommand.find(';');

        if (eqPos != std::string::npos) {
            dest = currentCommand.substr(0, eqPos);
            comp = currentCommand.substr(eqPos + 1, semiPos - eqPos - 1);
        } else {
            dest = "";
            comp = currentCommand.substr(0, semiPos);
        }

        if (semiPos != std::string::npos) {
            jump = currentCommand.substr(semiPos + 1);
        } else {
            jump = "";
        }
    }
};

class Code {
private:
    std::unordered_map<std::string, std::string> destTable;
    std::unordered_map<std::string, std::string> compTable;
    std::unordered_map<std::string, std::string> jumpTable;

public:
    Code() {
        initDestTable();
        initCompTable();
        initJumpTable();
    }

    std::string dest(const std::string& mnemonic) {
        return destTable[mnemonic];
    }

    std::string comp(const std::string& mnemonic) {
        return compTable[mnemonic];
    }

    std::string jump(const std::string& mnemonic) {
        return jumpTable[mnemonic];
    }

private:
    void initDestTable() {
        destTable[""] = "000";
        destTable["M"] = "001";
        destTable["D"] = "010";
        destTable["MD"] = "011";
        destTable["A"] = "100";
        destTable["AM"] = "101";
        destTable["AD"] = "110";
        destTable["AMD"] = "111";
    }

    void initCompTable() {
        compTable["0"] = "0101010";
        compTable["1"] = "0111111";
        compTable["-1"] = "0111010";
        compTable["D"] = "0001100";
        compTable["A"] = "0110000";
        compTable["M"] = "1110000";
        compTable["!D"] = "0001101";
        compTable["!A"] = "0110001";
        compTable["!M"] = "1110001";
        compTable["-D"] = "0001111";
        compTable["-A"] = "0110011";
        compTable["-M"] = "1110011";
        compTable["D+1"] = "0011111";
        compTable["A+1"] = "0110111";
        compTable["M+1"] = "1110111";
        compTable["D-1"] = "0001110";
        compTable["A-1"] = "0110010";
        compTable["M-1"] = "1110010";
        compTable["D+A"] = "0000010";
        compTable["D+M"] = "1000010";
        compTable["D-A"] = "0010011";
        compTable["D-M"] = "1010011";
        compTable["A-D"] = "0000111";
        compTable["M-D"] = "1000111";
        compTable["D&A"] = "0000000";
        compTable["D&M"] = "1000000";
        compTable["D|A"] = "0010101";
        compTable["D|M"] = "1010101";
    }

    void initJumpTable() {
        jumpTable[""] = "000";
        jumpTable["JGT"] = "001";
        jumpTable["JEQ"] = "010";
        jumpTable["JGE"] = "011";
        jumpTable["JLT"] = "100";
        jumpTable["JNE"] = "101";
        jumpTable["JLE"] = "110";
        jumpTable["JMP"] = "111";
    }
};

class Assembler {
private:
    SymbolTable symbolTable;
    Code code;

public:
    void assemble(const std::string& inputFile, const std::string& outputFile) {
        firstPass(inputFile);
        secondPass(inputFile, outputFile);
    }

private:
    void firstPass(const std::string& inputFile) {
        Parser parser(inputFile);
        int romAddress = 0;

        while (parser.hasMoreCommands()) {
            parser.advance();
            CommandType type = parser.getCommandType();
            if (type == L_COMMAND) {
                symbolTable.addEntry(parser.getSymbol(), romAddress);
            } else if (type != INVALID_COMMAND) {
                ++romAddress;
            }
        }
    }

    void secondPass(const std::string& inputFile, const std::string& outputFile) {
        Parser parser(inputFile);
        std::ofstream outFile(outputFile);

        while (parser.hasMoreCommands()) {
            parser.advance();
            CommandType type = parser.getCommandType();

            if (type == A_COMMAND) {
                std::string symbol = parser.getSymbol();
                int address;
                if (isdigit(symbol[0])) {
                    address = std::stoi(symbol);
                } else {
                    address = symbolTable.addVariable(symbol);
                }
                outFile << "0" + std::bitset<ADDRESS_LEN>(address).to_string() << "\n";
            } else if (type == C_COMMAND) {
                std::string dest = code.dest(parser.getDest());
                std::string comp = code.comp(parser.getComp());
                std::string jump = code.jump(parser.getJump());
                outFile << "111" + comp + dest + jump << "\n";
            }
        }

        outFile.close();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input file> <output file>\n";
        return 1;
    }

    Assembler assembler;
    assembler.assemble(argv[1], argv[2]);

    return 0;
}
