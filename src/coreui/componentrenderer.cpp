// SPDX-FileCopyrightText: 2026 Mattia Egloff <mattia.egloff@pm.me>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "componentrenderer.h"
#include "components/textcomponent.h"
#include "components/textinputcomponent.h"
#include "components/togglelistcomponent.h"
#include "components/fieldlistcomponent.h"
#include "components/cardpreviewcomponent.h"
#include "components/infopanelcomponent.h"
#include "components/contactlistcomponent.h"
#include "components/settingsgroupcomponent.h"
#include "components/actionlistcomponent.h"
#include "components/statusindicatorcomponent.h"
#include "components/pininputcomponent.h"
#include "components/qrcodecomponent.h"
#include "components/confirmationdialogcomponent.h"
#include "components/showtoastcomponent.h"
#include "components/inlineconfirmcomponent.h"
#include "components/editabletextcomponent.h"
#include "components/bannercomponent.h"
#include "components/dividercomponent.h"

#include <QLabel>

QWidget *ComponentRenderer::render(const QJsonObject &component,
                                   const OnAction &onAction) {
    // Component enum is serialized as { "VariantName": { ...fields } } or "Divider"
    if (component.contains("Text")) return TextComponent::render(component["Text"].toObject());
    if (component.contains("TextInput")) return TextInputComponent::render(component["TextInput"].toObject(), onAction);
    if (component.contains("ToggleList")) return ToggleListComponent::render(component["ToggleList"].toObject(), onAction);
    if (component.contains("FieldList")) return FieldListComponent::render(component["FieldList"].toObject(), onAction);
    if (component.contains("CardPreview")) return CardPreviewComponent::render(component["CardPreview"].toObject(), onAction);
    if (component.contains("InfoPanel")) return InfoPanelComponent::render(component["InfoPanel"].toObject());
    if (component.contains("ContactList")) return ContactListComponent::render(component["ContactList"].toObject(), onAction);
    if (component.contains("SettingsGroup")) return SettingsGroupComponent::render(component["SettingsGroup"].toObject(), onAction);
    if (component.contains("ActionList")) return ActionListComponent::render(component["ActionList"].toObject(), onAction);
    if (component.contains("StatusIndicator")) return StatusIndicatorComponent::render(component["StatusIndicator"].toObject());
    if (component.contains("PinInput")) return PinInputComponent::render(component["PinInput"].toObject(), onAction);
    if (component.contains("QrCode")) return QrcodeComponent::render(component["QrCode"].toObject());
    if (component.contains("ConfirmationDialog")) return ConfirmationDialogComponent::render(component["ConfirmationDialog"].toObject(), onAction);
    if (component.contains("ShowToast")) return ShowToastComponent::render(component["ShowToast"].toObject(), onAction);
    if (component.contains("InlineConfirm")) return InlineConfirmComponent::render(component["InlineConfirm"].toObject(), onAction);
    if (component.contains("EditableText")) return EditableTextComponent::render(component["EditableText"].toObject(), onAction);
    if (component.contains("Banner")) return BannerComponent::render(component["Banner"].toObject(), onAction);

    // Divider is serialized as just the string "Divider"
    return DividerComponent::render();
}
