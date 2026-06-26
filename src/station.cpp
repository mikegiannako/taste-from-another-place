#include "station.h"

const char* station_name(StationKind kind) {
    switch (kind) {
        case StationKind::BinTentacle: return "Tentacles";
        case StationKind::BinPotato:   return "Potatoes";
        case StationKind::BinBun:      return "Buns";
        case StationKind::Cutter:      return "Cutter";
        case StationKind::Drink:       return "Drinks";
        case StationKind::Grill:       return "Grill";
        case StationKind::Fryer:       return "Fryer";
    }
    return "?";
}

Color station_color(StationKind kind) {
    switch (kind) {
        case StationKind::BinTentacle: return (Color){180,  80, 120, 255};
        case StationKind::BinPotato:   return (Color){200, 170,  90, 255};
        case StationKind::BinBun:      return (Color){210, 180, 130, 255};
        case StationKind::Cutter:      return (Color){170, 170, 170, 255};
        case StationKind::Drink:       return (Color){100, 160, 220, 255};
        case StationKind::Grill:       return (Color){180,  60,  40, 255};
        case StationKind::Fryer:       return (Color){220, 160,  60, 255};
    }
    return GRAY;
}
