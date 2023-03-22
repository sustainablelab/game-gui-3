#ifndef __MG_COLORS_H__
#define __MG_COLORS_H__

namespace Colors
{ // 
    /* *************Colors from Steve Losh badwolf.vim***************
     *      _               _                 _  __
     *     | |__   __ _  __| | __      _____ | |/ _|
     *     | '_ \ / _` |/ _` | \ \ /\ / / _ \| | |_
     *     | |_) | (_| | (_| |  \ V  V / (_) | |  _|
     *     |_.__/ \__,_|\__,_|   \_/\_/ \___/|_|_|
     * 
     *      I am the Bad Wolf. I create myself.
     *       I take the words. I scatter them in time and space.
     *        A message to lead myself here.
     * 
     * A Vim colorscheme pieced together by Steve Losh.
     * Available at http://stevelosh.com/projects/badwolf/
     * *******************************/

    // Greys
    constexpr SDL_Color plain          = {0xf8,0xf6,0xf2,0xff};
    constexpr SDL_Color snow           = {0xff,0xff,0xff,0xff};
    constexpr SDL_Color coal           = {0x00,0x00,0x00,0xff};
    constexpr SDL_Color brightgravel   = {0xd9,0xce,0xc3,0xff};
    constexpr SDL_Color lightgravel    = {0x99,0x8f,0x84,0xff};
    constexpr SDL_Color gravel         = {0x85,0x7f,0x78,0xff};
    constexpr SDL_Color mediumgravel   = {0x66,0x64,0x62,0xff};
    constexpr SDL_Color deepgravel     = {0x45,0x41,0x3b,0xff};
    constexpr SDL_Color deepergravel   = {0x35,0x32,0x2d,0xff};
    constexpr SDL_Color darkgravel     = {0x24,0x23,0x21,0xff};
    constexpr SDL_Color blackgravel    = {0x1c,0x1b,0x1a,0xff};
    constexpr SDL_Color blackestgravel = {0x14,0x14,0x13,0xff};

    // Colors
    constexpr SDL_Color dalespale      = {0xfa,0xde,0x3e,0xff};
    constexpr SDL_Color dirtyblonde    = {0xf4,0xcf,0x86,0xff};
    constexpr SDL_Color taffy          = {0xff,0x2c,0x4b,0xff};
    constexpr SDL_Color saltwatertaffy = {0x8c,0xff,0xba,0xff};
    constexpr SDL_Color tardis         = {0x0a,0x9d,0xff,0xff};
    constexpr SDL_Color orange         = {0xff,0xa7,0x24,0xff};
    constexpr SDL_Color lime           = {0xae,0xee,0x00,0xff};
    constexpr SDL_Color dress          = {0xff,0x9e,0xb8,0xff};
    constexpr SDL_Color toffee         = {0xb8,0x88,0x53,0xff};
    constexpr SDL_Color coffee         = {0xc7,0x91,0x5b,0xff};
    constexpr SDL_Color darkroast      = {0x88,0x63,0x3f,0xff};

    const char* name[] = 
    {
            "coal",
            "blackestgravel",
            "blackgravel",
            "darkgravel",
            "deepergravel",
            "deepgravel",
            "gravel",
            "mediumgravel",
            "lightgravel",
            "brightgravel",
            "plain",
            "snow",
            "toffee",
            "coffee",
            "darkroast",
            "dalespale",
            "dirtyblonde",
            "lime",
            "taffy",
            "saltwatertaffy",
            "tardis",
            "orange",
            "dress",
    };
    constexpr SDL_Color list[] =
        {
            coal,
            blackestgravel,
            blackgravel,
            darkgravel,
            deepergravel,
            deepgravel,
            gravel,
            mediumgravel,
            lightgravel,
            brightgravel,
            plain,
            snow,
            toffee,
            coffee,
            darkroast,
            dalespale,
            dirtyblonde,
            lime,
            taffy,
            saltwatertaffy,
            tardis,
            orange,
            dress,
        };
    enum
    {
        COAL,
        BLACKESTGRAVEL,
        BLACKGRAVEL,
        DARKGRAVEL,
        DEEPERGRAVEL,
        DEEPGRAVEL,
        GRAVEL,
        MEDIUMGRAVEL,
        LIGHTGRAVEL,
        BRIGHTGRAVEL,
        PLAIN,
        SNOW,
        TOFFEE,
        COFFEE,
        DARKROAST,
        DALESPALE,
        DIRTYBLONDE,
        LIME,
        TAFFY,
        SALTWATERTAFFY,
        TARDIS,
        ORANGE,
        DRESS,
    };

    constexpr int count = sizeof(list)/sizeof(SDL_Color);
    void next(int& index)
    {
        index++;
        if (index >= count) index = 0;
    }
    void prev(int& index)
    {
        if (index == 0) index = count;
        index--;
    }

    int contrasts(int index)
    { // Return the index of the contrasting color

        constexpr int A_DARK  = DARKGRAVEL;
        constexpr int A_LIGHT = PLAIN;
        switch(index)
        {
            case SNOW:              return A_DARK;
            case COAL:              return A_LIGHT;
            case PLAIN:             return A_DARK;
            case BRIGHTGRAVEL:      return A_DARK;
            case LIGHTGRAVEL:       return A_DARK;
            case GRAVEL:            return A_DARK;
            case MEDIUMGRAVEL:      return A_LIGHT;     // >^.^<
            case DEEPGRAVEL:        return A_LIGHT;
            case DEEPERGRAVEL:      return A_LIGHT;
            case DARKGRAVEL:        return A_LIGHT;
            case BLACKGRAVEL:       return A_LIGHT;
            case BLACKESTGRAVEL:    return A_LIGHT;
            case DALESPALE:         return A_DARK;      // >^.^<
            case DIRTYBLONDE:       return A_DARK;
            case TAFFY:             return A_LIGHT;
            case SALTWATERTAFFY:    return A_DARK;
            case TARDIS:            return A_LIGHT;     // >^.^<
            case ORANGE:            return A_DARK;
            case LIME:              return A_DARK;
            case DRESS:             return A_DARK;
            case TOFFEE:            return A_DARK;
            case COFFEE:            return A_LIGHT;
            case DARKROAST:         return A_LIGHT;
            default:                return A_DARK;
        }
    }
}

#endif // __MG_COLORS_H__

