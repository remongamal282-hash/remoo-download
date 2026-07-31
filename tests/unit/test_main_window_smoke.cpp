#include "ui/main_window.h"
#include "ui/ipc_service_client.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QTimer>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

// Global QApplication required for any Qt widget test.
// We create a minimal one with no display if possible.
int    g_argc = 0;
char** g_argv = nullptr;

} // namespace

// ---------------------------------------------------------------------------
// Test: IpcServiceClient::getStatus returns empty list (not a crash) when
//       remo_service is NOT running.
// ---------------------------------------------------------------------------
TEST(GuiSmokeTest, IpcClientDisconnectedNocrash) {
    remo::ui::IpcServiceClient client;

    // Try connecting with a very short timeout — service is not running
    bool connected = client.connectToService("remo_download_ipc", 200);
    EXPECT_FALSE(connected) << "Should NOT connect — service is not running";

    // getStatus must return empty list, not crash
    auto statuses = client.getStatus(0);
    EXPECT_TRUE(statuses.empty()) << "Should return empty list when disconnected";

    EXPECT_FALSE(client.isConnected());
}

// ---------------------------------------------------------------------------
// Test: addDownload returns 0 (failure) when service is not running.
// ---------------------------------------------------------------------------
TEST(GuiSmokeTest, IpcClientAddDownloadDisconnected) {
    remo::ui::IpcServiceClient client;
    client.connectToService("remo_download_ipc", 200);

    int id = client.addDownload("https://example.com/test.zip", "test.zip",
                                "C:\\Downloads", "");
    EXPECT_EQ(id, 0) << "Should return 0 (fail) when service is offline";
}

// ---------------------------------------------------------------------------
// Test: MainWindow constructs and shows without crash even with no service.
//       This is a headless smoke test — it does not require a real display
//       because we use QApplication offscreen.
// ---------------------------------------------------------------------------
TEST(GuiSmokeTest, MainWindowConstructsWithoutCrash) {
    // MainWindow construction must not throw or crash
    EXPECT_NO_THROW({
        remo::ui::MainWindow window;
        // We do NOT call show() in headless mode to avoid display requirement
        // but we verify the object is created and destroyed cleanly.
    });
}

// ---------------------------------------------------------------------------
// Manual Verification Checklist (documented here for human review)
// ---------------------------------------------------------------------------
// The following scenarios require remo_service to be running and
// cannot be fully automated without a real display + event loop:
//
// MV-1: Launch remo_service, then RemooDownload.exe
//        → Status bar shows "● متصل بالخدمة" (green)
//
// MV-2: Launch RemooDownload.exe WITHOUT remo_service
//        → Status bar shows "● غير متصل بالخدمة" (red), no crash
//
// MV-3: Click "إضافة تحميل" → enter URL → "بدء التحميل"
//        → Dialog confirms "تمت الإضافة بنجاح (ID: X)"
//        → Within 1s the entry appears in the download table
//
// MV-4: Right-click a download row → "إيقاف"
//        → Status column changes to "متوقف" within 1s
//
// MV-5: Press X (close button) → window disappears
//        → System tray icon remains visible
//        → Double-clicking tray icon re-opens the window
//
// MV-6: System tray → "خروج" → application terminates completely
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // Create an offscreen QApplication for widget tests
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setLayoutDirection(Qt::RightToLeft);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
