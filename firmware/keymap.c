/* red: unit 018 part 4's stub, replaced by the generated table in the green commit */
#include "keymap.h"
const km_action *km_lookup(int key, int shift)
{
    static const km_action none = {KM_NONE, 0, 0, 0, ""};
    (void)key; (void)shift;
    return &none;
}
