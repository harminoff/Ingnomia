/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../localization/UiText.h"
#include <string>
namespace ingnomia::ui
{
// Domain catalogs may override these common failures. Unknown machine keys must
// not leak into player-facing status bars; already readable domain text is kept.
inline std::string commandFeedbackText(const localization::UiText& catalog, const std::string& value)
{
    if(value.empty()) return {};
    const LocalizationKey key{value};
    if(catalog.contains(key)) return catalog.format(key);
    static const localization::UiText common({
        {"ui.error.invalid_or_stale_action","The target changed or is no longer available. Select it again."},
        {"ui.error.bridge_unavailable","The game is not ready for this action."},
        {"ui.error.bridge_queue_failed","The game could not receive this action. Try again."},
        {"ui.error.confirmation_required","Review this action before continuing."},
        {"ui.error.invalid_payload","The action could not be applied. Check the values and try again."},
        {"ui.error.action_unavailable","This action is unavailable."},
        {"ui.error.action_rejected","The game could not apply this action. Try again."}
    });
    if(common.contains(key)) return common.format(key);
    return value.starts_with("ui.error.") ? common.format(LocalizationKey{"ui.error.action_rejected"}) : value;
}
}
