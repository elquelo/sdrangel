///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_FEATURE_JOGDIALCONTROLLERGUI_H_
#define INCLUDE_FEATURE_JOGDIALCONTROLLERGUI_H_

#include <QList>
#include <QPointer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "commands/commandkeyreceiver.h"
#include "settings/rollupstate.h"

#include "jogdialcontrollersettings.h"
#include "jogdialrangeslider.h"

class PluginAPI;
class FeatureUISet;
class JogdialController;
class Feature;
class QShortcut;
class QSlider;
class QLabel;
class QComboBox;
class QPushButton;

namespace Ui {
	class JogdialControllerGUI;
}

class JogdialControllerGUI : public FeatureGUI {
	Q_OBJECT
public:
	static JogdialControllerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
	virtual void destroy();

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

protected:
    void focusInEvent(QFocusEvent* e);
    void focusOutEvent(QFocusEvent *e);

private:
	Ui::JogdialControllerGUI* ui;
	PluginAPI* m_pluginAPI;
	FeatureUISet* m_featureUISet;
	JogdialControllerSettings m_settings;
    QList<QString> m_settingsKeys;
	RollupState m_rollupState;
	bool m_doApplySettings;

	JogdialController* m_jogdialController;
	MessageQueue m_inputMessageQueue;
	QList<JogdialControllerSettings::AvailableChannel> m_availableChannels;
	ChannelAPI *m_selectedChannel;
	CommandKeyReceiver m_commandKeyReceiver;
	QPointer<QShortcut> m_focusShortcut; //!< Takes the keyboard focus from any window of the application while the plugin is started

	JogdialRangeSlider *m_ringRange{nullptr};   //!< Speed of the first and last ring positions
	QLabel *m_ringRangeValue{nullptr};
	QSlider *m_reaction{nullptr};
	QLabel *m_reactionValue{nullptr};
	QSlider *m_jogStep{nullptr};
	QLabel *m_jogStepValue{nullptr};
	QSlider *m_jogAccel{nullptr};
	QLabel *m_jogAccelValue{nullptr};
	QComboBox *m_motionMode{nullptr};
	QPushButton *m_motionDefaults{nullptr};
	JogdialScale *m_speedScale{nullptr};
	JogdialScale *m_reactionScale{nullptr};
	JogdialScale *m_clickScale{nullptr};
	JogdialScale *m_accelScale{nullptr};
	int m_displayedMode{0};

	explicit JogdialControllerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
	virtual ~JogdialControllerGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	void updateChannelList();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void takeKeyboardFocus();
	void buildMotionUI();
	void displayMotion();
	void displayMotionTexts();
	void motionChanged();
	double ringValue(int axis) const;
	int ringAxis(double value) const;
	double jogStepValue(int slider) const;
	int jogStepSlider(double value) const;
	void displayMotionScales();
	static QString formatSpeed(int mode, double value);
	static double roundToOneDigit(double value);

private slots:
	void onMenuDialogCalled(const QPoint &p);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
	void on_startStop_toggled(bool checked);
	void on_devicesRefresh_clicked();
	void on_channels_currentIndexChanged(int index);
	void updateFeatureState();
	void tick();
};


#endif // INCLUDE_FEATURE_JOGDIALCONTROLLERGUI_H_
