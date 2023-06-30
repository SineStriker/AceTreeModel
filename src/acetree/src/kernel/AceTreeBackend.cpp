#include "AceTreeBackend.h"

AceTreeBackend::AceTreeBackend(QObject *parent) : QObject(parent) {
}

AceTreeBackend::~AceTreeBackend() {
}

QVariantHash AceTreeBackend::modelInfo() const {
    return {};
}

void AceTreeBackend::setModelInfo(const QVariantHash &info) {
    Q_UNUSED(info);
}
