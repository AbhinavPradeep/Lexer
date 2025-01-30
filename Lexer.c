#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "GenerateNFA.h"

// For marking StateSetList membership
static int Generation = 0;

#define MAX_LINE_LENGTH 256
#define INITIAL_SIZE_NFA 32
#define INITIAL_SIZE_TOK 128

typedef struct CombinedNFA{
    State** SubNFA;
    int SizeOfStructure;
    int Number;
} CombinedNFA;

typedef struct {
    char* Class;
    char* Lexeme;
} Token;

typedef struct {
    Token* Tokens;
    int SizeOfStructure;
    int Number;
} TokenArray;

void InitializeTokenArray(TokenArray* Array) {
    Array->Tokens = malloc(INITIAL_SIZE_TOK*sizeof(Token));
    Array->SizeOfStructure = INITIAL_SIZE_TOK;
    Array->Number = 0;
}

void InsertToken(TokenArray* Array, char* Class, char* Start, char* End) {
    if (Array->SizeOfStructure == Array->Number) {
        Array->SizeOfStructure = Array->SizeOfStructure*2;
        Array->Tokens = realloc(Array->Tokens, Array->SizeOfStructure * sizeof(Token));
    }

    Array->Tokens[Array->Number].Class = strdup(Class);

    int LexemeLength = (End - Start) + 1;
    Array->Tokens[Array->Number].Lexeme = malloc(LexemeLength + 1);
    memcpy(Array->Tokens[Array->Number].Lexeme, Start, LexemeLength);
    Array->Tokens[Array->Number].Lexeme[LexemeLength] = '\0';

    Array->Number++;
}

void FreeTokenArray(TokenArray* Array) {
    for (int i = 0; i < Array->Number; i++) {
        free(Array->Tokens[i].Class);
        free(Array->Tokens[i].Lexeme);
    }
    free(Array->Tokens);
}

CombinedNFA* ProcessLexicalGrammar(const char *Name) {

    CombinedNFA* NFA = malloc(sizeof(CombinedNFA));

    NFA->SubNFA = malloc(sizeof(State*)*INITIAL_SIZE_NFA);
    NFA->SizeOfStructure = INITIAL_SIZE_NFA;
    NFA->Number = 0;

    FILE *File = fopen(Name, "r");

    if (!File) {
        perror("Error opening file");
        exit(1);
    }

    char Line[MAX_LINE_LENGTH];
    while (fgets(Line, sizeof(Line), File)) {
        char* TokenName = strtok(Line, ":");
        char* Regex = strtok(NULL, "\n");

        if (TokenName && Regex) {
            // printf("<Token: %s, Regex: %s>\n", TokenName, Regex);
            SubExpression Sub = GenerateNFA(Regex, TokenName);
            NFA->SubNFA[NFA->Number] = Sub.Start;
        }

        NFA->Number++;

        if(NFA->Number == NFA->SizeOfStructure) {
            NFA->SizeOfStructure = NFA->SizeOfStructure*2;
            NFA->SubNFA = realloc(NFA->SubNFA, sizeof(State*)*NFA->SizeOfStructure);
        }
    }

    fclose(File);

    return NFA;
}

void Add(StateSetList* Set, State* S) {
    if (S == NULL || S->Mark == Generation) {
        return;
    }
    S->Mark = Generation;
    //Ensure you do $\varepsilon$-closure
    if(CheckStateOut(Split,S)) {
        Add(Set, S->Transition1);
        Add(Set, S->Transition2);
        return;
    }
    Set->States[Set->Size] = S;
    Set->Size++;
}

int SetContainsMatch(StateSetList* Set) {
    for(int i = 0; i < Set->Size; i++) {
        if(CheckStateOut(Match, Set->States[i])) {
            printf("This is a %s token.\n", Set->States[i]->TokenClass);
            return 1;
        }
    }
    return 0;
}

void StepThroughNFA(StateSetList* CurrentSet, StateSetList* NextSet, int Symbol) {
    Generation++;
    State* S;
    NextSet->Size = 0;
    for(int i = 0; i < CurrentSet->Size; i++) {
        S = CurrentSet->States[i];
        if(CheckStateOut(Symbol,S)) {
            // Will never have Split states as Add 
            // always ensures that $\varepsilon$-closure is done
            Add(NextSet, S->Transition1);
        }
    }
}

int MatchesRegex(CombinedNFA* NFA, char* StringToCheck) {
    StateSetList CurrentSet, NextSet;
    CurrentSet.Size = 0;
    Generation++;
    for (int i = 0; i < NFA->Number; i++)
    {
        Add(&CurrentSet, NFA->SubNFA[i]);
    }

    NextSet.Size = 0;

    for (char* SymbolPointer = StringToCheck; *SymbolPointer != '\0'; SymbolPointer++) {
        StepThroughNFA(&CurrentSet, &NextSet, *SymbolPointer);
        StateSetList Temp = CurrentSet;
        CurrentSet = NextSet;
        NextSet = Temp;
    }
    return SetContainsMatch(&CurrentSet);
}

TokenArray* TokenizeText(CombinedNFA* NFA, char* Text) {
// void TokenizeText(CombinedNFA* NFA, char* Text) {
    char* Start = Text;
    char* Forward;
    
    TokenArray* TokenizedText = malloc(sizeof(TokenArray));
    InitializeTokenArray(TokenizedText);
    StateSetList CurrentSet, NextSet;

    // Pre-compute \varepsilon-closure of start.
    StateSetList InitialSet;
    InitialSet.Size = 0;
    Generation++;
    for (int i = 0; i < NFA->Number; i++) {
        Add(&InitialSet, NFA->SubNFA[i]);
    }

    char* LastMatchedToken;
    char* LastMatchedPosition;
    
    while (*Start != '\0')
    {
        Forward = Start;
        
        LastMatchedToken = NULL;
        LastMatchedPosition = NULL;

        // Reset to inital state;
        CurrentSet.Size = InitialSet.Size;
        memcpy(CurrentSet.States, InitialSet.States, InitialSet.Size * sizeof(State*));

        while (*Forward != '\0')
        {
            NextSet.Size = 0;

            StepThroughNFA(&CurrentSet, &NextSet, *Forward);

            if (NextSet.Size == 0) {
                break;
            }

            // Check for match (still naive)
            for(int i = 0; i < NextSet.Size; i++) {
                if(CheckStateOut(Match, NextSet.States[i])) {
                    LastMatchedToken = NextSet.States[i]->TokenClass;
                    LastMatchedPosition = Forward;
                }
            }

            StateSetList Temp = CurrentSet;
            CurrentSet = NextSet;
            NextSet = Temp;

            Forward++;
        }

        if (LastMatchedToken != NULL) {

            if (strcmp(LastMatchedToken, "WHITE") == 0 || strcmp(LastMatchedToken, "NEWLINE") == 0) {
                // Move past whitespace
                Start = LastMatchedPosition + 1;
                // Skip token
                continue;
            }

            InsertToken(TokenizedText, LastMatchedToken, Start, LastMatchedPosition);

            // printf("<Token: %s, Lexeme: \"", LastMatchedToken);
            // for (char* p = Start; p <= LastMatchedPosition; p++) {
            //     putchar(*p);
            // }
            // printf("\">\n");

            Start = LastMatchedPosition + 1;

        } else {
            InsertToken(TokenizedText, "ERROR", Start, Start);
            // printf("Lexical error: Unrecognized token at \"%c\"\n", *Start);
            Start++;
        }
        
    }
    
    return TokenizedText;
}

int main(int argc, char *argv[]) {

    if (argc < 3) {
        printf("Usage: lex <grammar_file> <input_file>\n");
        return 1;
    }

    CombinedNFA* NFA = ProcessLexicalGrammar(argv[1]);

    FILE* File = fopen(argv[2], "r");

    if (!File) {
        perror("Error opening input file");
        return 1;
    }

    fseek(File, 0, SEEK_END);

    long File_size = ftell(File);
    
    rewind(File);

    char* Buffer = malloc(File_size + 1);

    fread(Buffer, 1, File_size, File);

    Buffer[File_size] = '\0';

    fclose(File);

    TokenArray* TokenizedText = TokenizeText(NFA, Buffer);

    for (int i = 0; i < TokenizedText->Number; i++) {
        printf("<Token: %s, Lexeme: \"%s\">\n", TokenizedText->Tokens[i].Class, TokenizedText->Tokens[i].Lexeme);
    }

    free(Buffer);

    return 0;
}