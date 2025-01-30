#define MAX_SIZE 200

enum { 
    Match = 256, 
    Split = 257 
};

// Forward declarations
typedef struct state State;
typedef struct sub SubExpression;
typedef struct ptrlist PointerList;
typedef struct ssl StateSetList;

// Structures
struct state {
    int SymbolRangeStart, SymbolRangeEnd;
    State *Transition1, *Transition2;
    int Mark;
    char* TokenClass;
};

struct ptrlist {
    State **CurrentPointer;
    PointerList *NextPointer;
};

struct ssl {
    State *States[MAX_SIZE];
    int Size;
};

struct sub {
    State *Start;
    PointerList *OutPointers;
};

SubExpression GenerateNFA(char* Regex, char* Token);
bool CheckStateOut(int Symbol, State* State);
void FreeSubExpression(SubExpression* expr);