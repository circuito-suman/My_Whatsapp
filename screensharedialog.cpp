#include "screensharedialog.h"
#include <QIcon>
#include <QDebug>

ScreenShareDialog::ScreenShareDialog(const QWebEngineDesktopMediaRequest &request, QWidget *parent)
    : QDialog(parent)
    , m_request(request)
    , m_tabWidget(nullptr)
    , m_screensView(nullptr)
    , m_windowsView(nullptr)
    , m_statusLabel(nullptr)
    , m_shareButton(nullptr)
    , m_cancelButton(nullptr)
    , m_handled(false)
{
    setWindowTitle(tr("Share Screen - WhatsApp Desktop"));
    setWindowIcon(QIcon(":/icons/whatsapp.png"));
    setMinimumSize(520, 380);
    setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_statusLabel = new QLabel(tr("Select a screen or window to share with WhatsApp:"), this);
    QFont font = m_statusLabel->font();
    font.setBold(true);
    m_statusLabel->setFont(font);
    mainLayout->addWidget(m_statusLabel);

    m_tabWidget = new QTabWidget(this);

    // Screens tab
    m_screensView = new QListView(this);
    m_screensView->setViewMode(QListView::IconMode);
    m_screensView->setIconSize(QSize(120, 80));
    m_screensView->setGridSize(QSize(140, 110));
    m_screensView->setResizeMode(QListView::Adjust);
    m_screensView->setMovement(QListView::Static);
    m_screensView->setSelectionMode(QAbstractItemView::SingleSelection);

    if (m_request.screensModel()) {
        m_screensView->setModel(m_request.screensModel());
        connect(m_request.screensModel(), &QAbstractItemModel::rowsInserted, this, &ScreenShareDialog::updateSelectionAndState);
        connect(m_request.screensModel(), &QAbstractItemModel::modelReset, this, &ScreenShareDialog::updateSelectionAndState);
        connect(m_request.screensModel(), &QAbstractItemModel::layoutChanged, this, &ScreenShareDialog::updateSelectionAndState);
        connect(m_request.screensModel(), &QAbstractItemModel::rowsRemoved, this, &ScreenShareDialog::updateSelectionAndState);
    }
    m_tabWidget->addTab(m_screensView, tr("Entire Screen"));

    // Windows tab
    m_windowsView = new QListView(this);
    m_windowsView->setViewMode(QListView::IconMode);
    m_windowsView->setIconSize(QSize(120, 80));
    m_windowsView->setGridSize(QSize(140, 110));
    m_windowsView->setResizeMode(QListView::Adjust);
    m_windowsView->setMovement(QListView::Static);
    m_windowsView->setSelectionMode(QAbstractItemView::SingleSelection);

    if (m_request.windowsModel()) {
        m_windowsView->setModel(m_request.windowsModel());
        connect(m_request.windowsModel(), &QAbstractItemModel::rowsInserted, this, &ScreenShareDialog::updateSelectionAndState);
        connect(m_request.windowsModel(), &QAbstractItemModel::modelReset, this, &ScreenShareDialog::updateSelectionAndState);
        connect(m_request.windowsModel(), &QAbstractItemModel::layoutChanged, this, &ScreenShareDialog::updateSelectionAndState);
        connect(m_request.windowsModel(), &QAbstractItemModel::rowsRemoved, this, &ScreenShareDialog::updateSelectionAndState);
    }
    m_tabWidget->addTab(m_windowsView, tr("Application Window"));

    mainLayout->addWidget(m_tabWidget);

    QLabel *noticeLabel = new QLabel(tr("Desktop screen and window sharing uses the native PipeWire portal under Wayland."), this);
    noticeLabel->setWordWrap(true);
    noticeLabel->setStyleSheet("color: #888; font-style: italic; font-size: 11px;");
    mainLayout->addWidget(noticeLabel);

    // Bottom action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_shareButton = new QPushButton(tr("Share"), this);
    m_shareButton->setDefault(true);
    m_shareButton->setStyleSheet("QPushButton { background-color: #25D366; color: white; font-weight: bold; border-radius: 4px; padding: 6px 16px; }"
                                 "QPushButton:hover { background-color: #20BA5C; }"
                                 "QPushButton:disabled { background-color: #888888; color: #CCCCCC; }");

    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_shareButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_shareButton, &QPushButton::clicked, this, &ScreenShareDialog::onShareClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &ScreenShareDialog::onCancelClicked);
    connect(m_screensView, &QListView::doubleClicked, this, &ScreenShareDialog::onItemDoubleClicked);
    connect(m_windowsView, &QListView::doubleClicked, this, &ScreenShareDialog::onItemDoubleClicked);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &ScreenShareDialog::onTabChanged);

    if (m_screensView->selectionModel()) {
        connect(m_screensView->selectionModel(), &QItemSelectionModel::currentChanged, this, &ScreenShareDialog::updateSelectionAndState);
    }
    if (m_windowsView->selectionModel()) {
        connect(m_windowsView->selectionModel(), &QItemSelectionModel::currentChanged, this, &ScreenShareDialog::updateSelectionAndState);
    }

    updateSelectionAndState();
}

ScreenShareDialog::~ScreenShareDialog()
{
    if (!m_handled) {
        m_request.cancel();
        m_handled = true;
    }
}

void ScreenShareDialog::reject()
{
    if (!m_handled) {
        m_request.cancel();
        m_handled = true;
    }
    QDialog::reject();
}

void ScreenShareDialog::onCancelClicked()
{
    reject();
}

void ScreenShareDialog::onTabChanged(int index)
{
    Q_UNUSED(index)
    updateSelectionAndState();
}

void ScreenShareDialog::updateSelectionAndState()
{
    bool isScreensTab = (m_tabWidget->currentIndex() == 0);
    QAbstractListModel *activeModel = isScreensTab ? m_request.screensModel() : m_request.windowsModel();
    QListView *activeView = isScreensTab ? m_screensView : m_windowsView;

    int rowCount = (activeModel != nullptr) ? activeModel->rowCount() : 0;

    if (rowCount > 0) {
        if (!activeView->currentIndex().isValid()) {
            activeView->setCurrentIndex(activeModel->index(0, 0));
        }
        m_shareButton->setEnabled(true);
        if (m_statusLabel) {
            m_statusLabel->setText(isScreensTab ? tr("Select a screen to share with WhatsApp:")
                                                : tr("Select an application window to share with WhatsApp:"));
        }
    } else {
        // Model may still be enumerating via PipeWire / X11
        m_shareButton->setEnabled(false);
        if (m_statusLabel) {
            m_statusLabel->setText(isScreensTab ? tr("Enumerating available screens via display server / PipeWire...")
                                                : tr("Enumerating application windows via display server / PipeWire..."));
        }
    }
}

void ScreenShareDialog::onShareClicked()
{
    if (m_handled) return;

    // C++20 designated initializers for capture stream profile parameters
    constexpr CaptureStreamConfig streamConfig{
        .targetFps = 30,
        .maxResolutionWidth = 1920,
        .maxResolutionHeight = 1080,
        .enableAudioShare = true
    };
    Q_UNUSED(streamConfig);

    if (m_tabWidget->currentIndex() == 0) {
        // Screens tab
        QModelIndex current = m_screensView->currentIndex();
        if (!current.isValid() && m_screensView->selectionModel()) {
            const QModelIndexList selected = m_screensView->selectionModel()->selectedIndexes();
            if (!selected.isEmpty()) {
                current = selected.first();
            }
        }
        if (current.isValid()) {
            qDebug() << "ScreenShareDialog: Sharing selected screen at index:" << current.row();
            m_request.selectScreen(current);
            m_handled = true;
            accept();
            return;
        } else if (m_request.screensModel() && m_request.screensModel()->rowCount() > 0) {
            qDebug() << "ScreenShareDialog: Sharing primary screen at index 0";
            m_request.selectScreen(m_request.screensModel()->index(0, 0));
            m_handled = true;
            accept();
            return;
        }
    } else {
        // Windows tab
        QModelIndex current = m_windowsView->currentIndex();
        if (!current.isValid() && m_windowsView->selectionModel()) {
            const QModelIndexList selected = m_windowsView->selectionModel()->selectedIndexes();
            if (!selected.isEmpty()) {
                current = selected.first();
            }
        }
        if (current.isValid()) {
            qDebug() << "ScreenShareDialog: Sharing selected window at index:" << current.row();
            m_request.selectWindow(current);
            m_handled = true;
            accept();
            return;
        } else if (m_request.windowsModel() && m_request.windowsModel()->rowCount() > 0) {
            qDebug() << "ScreenShareDialog: Sharing window at index 0";
            m_request.selectWindow(m_request.windowsModel()->index(0, 0));
            m_handled = true;
            accept();
            return;
        }
    }

    qWarning() << "ScreenShareDialog: No screen or window was available to share yet.";
}

void ScreenShareDialog::onItemDoubleClicked(const QModelIndex &index)
{
    if (m_tabWidget->currentIndex() == 0) {
        m_screensView->setCurrentIndex(index);
    } else {
        m_windowsView->setCurrentIndex(index);
    }
    onShareClicked();
}
