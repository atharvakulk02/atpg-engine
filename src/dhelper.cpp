#include "../include/dhelper.h"

int goodcircuit(LogicValue v){
    switch(v){
        case D:return 1;
        case D_BAR:return 0;
        case ONE:return 1;
        case ZERO:return 0;
        case X:return -1;
        default:return -1;
    }
}

int badcircuit(LogicValue v){
    switch(v){
        case D:return 0;
        case D_BAR:return 1;
        case ONE:return 1;
        case ZERO:return 0;
        case X:return -1;
        default:return -1;
    }
}

LogicValue combval(int g, int b){
    if ((g==0)&&(b==0)){
        return ZERO;
    }
    else if ((g==0)&&(b==1)){
        return D_BAR;
    }
    else if ((g==1)&&(b==0)){
        return D;
    }
    else if ((g==1)&&(b==1)){
        return ONE;
    }
    else if ((g==-1)or(b==-1)){
        return X;
    }
    else{
        return X;
    }
}