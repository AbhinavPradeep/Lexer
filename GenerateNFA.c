#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct state State;
typedef struct sub SubExpression;
typedef struct ptrlist PointerList;
typedef struct ssl StateSetList;

#define MAX_SIZE 200

// For marking StateSetList membership
static int Generation = 0;

// 0-225 is for all ASCII symbols. Therefore
// We take 256 to denote a Match state.
// We take 257 to denote a Split state.
enum
{
	Match = 256,
	Split = 257
};

struct state {
    int SymbolRangeStart;
    int SymbolRangeEnd;
    State* Transition1;  
    State* Transition2;
    // For checking StateSetList membership
    int Mark;
    char* TokenClass;
};

struct ptrlist {
    State** CurrentPointer;
    PointerList* NextPointer;
};

struct ssl
{
    // For simplicity we keep a statically sized
    // container.
	State* States[MAX_SIZE];
	int Size;
};


struct sub {
    State* Start;
    // Out pointers of the accept states in the fragment
    PointerList* OutPointers;
};

State* CreateState(int SymbolRangeStart, int SymbolRangeEnd, State* Transition1, State* Transition2) {
    State* NewState = malloc(sizeof(State));
    //printf("Created state -> %p\n", (void*)NewState);
    NewState->SymbolRangeStart = SymbolRangeStart;
    NewState->SymbolRangeEnd = SymbolRangeEnd;
    NewState->Transition1 = Transition1;
    NewState->Transition2 = Transition2;
    NewState->Mark = 0; 
    return NewState;
}

bool CheckStateOut(int Symbol, State* State) {
    return (Symbol >= State->SymbolRangeStart && Symbol <= State->SymbolRangeEnd);
}

SubExpression CreateSubExpression(State* Start, PointerList* OutPointers)
{
	SubExpression New;
    New.Start = Start;
    New.OutPointers = OutPointers;
    return New;
}

void ConnectAutomata(PointerList* OutPointers, State* Start)
{
    PointerList* Next;
    while (OutPointers != NULL) {
        Next = OutPointers->NextPointer;
        *(OutPointers->CurrentPointer) = Start;
        OutPointers = Next;
    }

}

PointerList* AppendOutPointers(PointerList* OutPointers, PointerList* MoreOutPointers)
{
    if (OutPointers == NULL) {
        //printf("Appending NULL -> %p\n", (void*)MoreOutPointers);
        return MoreOutPointers;
    }
	PointerList* NewOutPointers = OutPointers;
    while (OutPointers->NextPointer != NULL)
    {
        OutPointers = OutPointers->NextPointer;
    }
    OutPointers->NextPointer = MoreOutPointers;
    //printf("Appending %p -> %p\n", (void*)OutPointers, (void*)MoreOutPointers);
    return NewOutPointers;
}

void FreePointerList(PointerList* list) {
    while (list) {
        PointerList* next = list->NextPointer;
        //printf("Freeing PointerList at %p\n", (void*)list);
        free(list);
        list = next;
    }
}

SubExpression ApplyKleeneStar(SubExpression* NFA) {
    State* NewStart = CreateState(Split, Split, NFA->Start, NULL);
    ConnectAutomata(NFA->OutPointers, NewStart);
    FreePointerList(NFA->OutPointers);
    // Make a new PointerList with just NewStart->Transition2
    PointerList* NewOutPointers = malloc(sizeof(PointerList));
    NewOutPointers->CurrentPointer = &(NewStart->Transition2);
    NewOutPointers->NextPointer = NULL;
    return CreateSubExpression(NewStart,NewOutPointers);
}

SubExpression ApplyOneOrMore(SubExpression* NFA) {
    State* NewSplit = CreateState(Split, Split, NFA->Start, NULL);
    ConnectAutomata(NFA->OutPointers, NewSplit);
    FreePointerList(NFA->OutPointers);
    // Make a new PointerList with just NewSplit->Transition2
    PointerList* NewOutPointers = malloc(sizeof(PointerList));
    NewOutPointers->CurrentPointer = &(NewSplit->Transition2);
    NewOutPointers->NextPointer = NULL;
    return CreateSubExpression(NFA->Start,NewOutPointers);
}

SubExpression ApplyZeroOrOne(SubExpression* NFA) {
    State* NewStart = CreateState(Split, Split, NFA->Start, NULL);
    PointerList* NewOutPointer = malloc(sizeof(PointerList));
    NewOutPointer->CurrentPointer = &(NewStart->Transition2);
    NewOutPointer->NextPointer = NULL;
    return CreateSubExpression(NewStart,AppendOutPointers(NFA->OutPointers,NewOutPointer));
}

SubExpression ApplyUnion(SubExpression* NFA1, SubExpression* NFA2) {
    State* NewStart = CreateState(Split, Split, NFA1->Start, NFA2->Start);
    return CreateSubExpression(NewStart, AppendOutPointers(NFA1->OutPointers, NFA2->OutPointers));
}

SubExpression ApplyConcatenation(SubExpression* NFA1, SubExpression* NFA2) {
    ConnectAutomata(NFA1->OutPointers, NFA2->Start);
    FreePointerList(NFA1->OutPointers);
    return CreateSubExpression(NFA1->Start, NFA2->OutPointers);
}

SubExpression CreateSingleCharacter(int Symbol) {
    State* Start = CreateState(Symbol, Symbol, NULL, NULL);
    PointerList* OutPointer = malloc(sizeof(PointerList));
    //printf("Allocated PointerList at %p\n", (void*)OutPointer);
    OutPointer->CurrentPointer = &(Start->Transition1);
    OutPointer->NextPointer = NULL;
    return CreateSubExpression(Start, OutPointer);
}

SubExpression CreateRange(int StartSymbol, int EndSymbol) {
    State* Start = CreateState(StartSymbol, EndSymbol, NULL, NULL);
    PointerList* OutPointer = malloc(sizeof(PointerList));
    //printf("Allocated PointerList at %p\n", (void*)OutPointer);
    OutPointer->CurrentPointer = &(Start->Transition1);
    OutPointer->NextPointer = NULL;
    return CreateSubExpression(Start, OutPointer);
}

bool ParseCharacterRange(char** S, int* SymbolRange) {
    // Check if we are a valid character and 
    // if the next character is '-'
    if(!(**S >= 32 && **S <= 255 && *(*S+1) == '-')) return false;
    SymbolRange[0] = **S;
    *S+=2;
    // Check if we are a valid character and 
    // if the next character is ']'
    if(!(**S >= 32 && **S <= 255 && *(*S+1) == ']')) return false;
    SymbolRange[1] = **S;
    *S+=1;
    return true;
}

SubExpression GenerateNFA(char* Regex, char* TokenClass) {
    SubExpression Stack[1000];
    SubExpression* StackPointer = Stack;
    SubExpression E1, E2, E;
    int SymbolRange[2]  = {0, 0};
    char* Symbol = Regex;

    #define push(s) (*StackPointer++ = s)
    #define pop() (*--StackPointer)
    
    while (*Symbol != '\0')
    {
        
        if (*Symbol == '.')
        {
            // Concatenate
            E2 = pop();
            E1 = pop();
            E = ApplyConcatenation(&E1, &E2);
            push(E);
        } else if (*Symbol == '|') {
            E1 = pop();
            E2 = pop();
            E = ApplyUnion(&E1, &E2);
            push(E);
            // Union
        } else if (*Symbol == '*') {
            // Kleene star
            E = pop();
            E = ApplyKleeneStar(&E);
            push(E);
        } else if (*Symbol == '+') {
            // One or more
            E = pop();
            E = ApplyOneOrMore(&E);
            push(E);
        } else if (*Symbol == '?') {
            // Zero or one
            E = pop();
            E = ApplyZeroOrOne(&E);
            push(E);
        } else if (*Symbol == '\\') {
            Symbol++;
            if (*Symbol == 't') {
                E = CreateSingleCharacter('\t');
                push(E);
            } else if (*Symbol == 'n') {
                E = CreateSingleCharacter('\n');
                push(E);
            } else if (*Symbol == 'r') {
                E = CreateSingleCharacter('\r');
                push(E);
            } else if (*Symbol >= 32 && *Symbol <= 255) {
                // Any printable ASCII character should 
                // become a single state
                E = CreateSingleCharacter(*Symbol);
                push(E);
            } else {
                // Weird symbol
                fprintf(stderr, "Invalid symbol '%c' in regex.\n", *Symbol);
                exit(1);
            }
        } else if (*Symbol == '[') {
            Symbol++;
            // Ensure pattern matches "(start)-(end)]"
            if (ParseCharacterRange(&Symbol, SymbolRange)) {
                //fprintf(stderr, "At %c \n", *Symbol);
                E = CreateRange(SymbolRange[0], SymbolRange[1]);
                push(E);
            } else {
                fprintf(stderr, "Character range was specified incorrectly.");
                exit(1);
            }
        } else if (*Symbol >= 32 && *Symbol <= 255) {
            // Any printable ASCII character should 
            // become a single state
            E = CreateSingleCharacter(*Symbol);
            push(E);
        } else {
            // Weird symbol
            fprintf(stderr, "Invalid symbol '%c' in regex.\n", *Symbol);
            exit(1);
        }
        // Continue reading string
        Symbol++;
    }

    // If your stack size is not 1, then something went wrong.
    if (StackPointer - Stack != 1) {
        fprintf(stderr, "Something went wrong, stack has %ld items left.\n", StackPointer - Stack);
        exit(1);
    }

    E = pop();
    State* MatchState = CreateState(Match, Match, NULL, NULL);
    MatchState->TokenClass = strdup(TokenClass);
    ConnectAutomata(E.OutPointers, MatchState);
    // return pop();
    return E;
}