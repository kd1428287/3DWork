#pragma once

#include "Event.h"

// 再生中の音を指すID(0は無効)
struct SoundHandle
{
	uint32_t id = 0;

	bool IsValid() const { return id != 0; }

	// 呼び出し側が事前に発行しておき、後から停止などに使う
	static SoundHandle Issue()
	{
		static std::atomic<uint32_t> s_next{ 1 };
		return SoundHandle{ s_next.fetch_add(1) };
	}
};

// 音の種別(種別ごとに音量を調整できる)
enum class SoundCategory { SE, BGM, Voice, Max };

namespace Events
{
	// 音の再生が要求された
	struct SoundPlayRequested : public Event {
		std::string soundId;			// 論理ID(未登録ならファイル名として扱う)
		Math::Vector3 pos = {};
		bool is3D = false;
		bool loop = false;
		float volume = 1.0f;
		float pitch = 0.0f;
		float fadeIn = 0.0f;			// フェードイン時間(秒)。0以下ならフェードしない
		SoundHandle handle;			// 無効なら再生側で発行する
	};

	// 指定した音の停止が要求された
	struct SoundStopRequested : public Event {
		SoundHandle handle;
	};

	// 指定した音の音量変更が要求された
	struct SoundVolumeChangeRequested : public Event {
		SoundHandle handle;
		float volume = 1.0f;
	};

	// 指定した音のフェードが要求された(現在の音量から目標音量へ線形に変化)
	struct SoundFadeRequested : public Event {
		SoundHandle handle;
		float targetVolume = 0.0f;
		float duration = 1.0f;			// 秒
		bool stopOnEnd = false;			// 完了時に停止する(フェードアウト用)
	};

	// 種別ごとの音量変更が要求された
	struct SoundCategoryVolumeChangeRequested : public Event {
		SoundCategory category = SoundCategory::SE;
		float volume = 1.0f;
	};

	// 全ての音への一括操作が要求された
	struct StopAllSoundRequested : public Event {};
	struct PauseAllSoundRequested : public Event {};
	struct ResumeAllSoundRequested : public Event {};

	// 再生中の音の停止とアセット解放が要求された(シーン切替時など)
	struct SoundResetRequested : public Event {};

	// 音の再生が終わった(停止された)
	struct SoundFinished : public Event {
		SoundHandle handle;
	};

	inline SoundHandle PublishSoundPlayEvent(EventBus& bus, std::string_view soundId, bool loop = false, float volume = 1.0f, float pitch = 0.0f, float fadeIn = 0.0f)
	{
		SoundPlayRequested e;
		e.soundId = soundId;
		e.loop = loop;
		e.volume = volume;
		e.pitch = pitch;
		e.fadeIn = fadeIn;
		e.handle = SoundHandle::Issue();
		bus.Publish(e);

		return e.handle;
	}

	inline SoundHandle PublishSoundPlay3DEvent(EventBus& bus, std::string_view soundId, const Math::Vector3& pos, bool loop = false, float volume = 1.0f, float pitch = 0.0f, float fadeIn = 0.0f)
	{
		SoundPlayRequested e;
		e.soundId = soundId;
		e.pos = pos;
		e.is3D = true;
		e.loop = loop;
		e.volume = volume;
		e.pitch = pitch;
		e.fadeIn = fadeIn;
		e.handle = SoundHandle::Issue();
		bus.Publish(e);

		return e.handle;
	}

	inline void PublishSoundStopEvent(EventBus& bus, SoundHandle handle)
	{
		SoundStopRequested e;
		e.handle = handle;
		bus.Publish(e);
	}

	inline void PublishSoundCategoryVolumeChangeEvent(EventBus& bus, SoundCategory category, float volume)
	{
		SoundCategoryVolumeChangeRequested e;
		e.category = category;
		e.volume = volume;
		bus.Publish(e);
	}

	inline void PublishSoundVolumeChangeEvent(EventBus& bus, SoundHandle handle, float volume)
	{
		SoundVolumeChangeRequested e;
		e.handle = handle;
		e.volume = volume;
		bus.Publish(e);
	}

	// 目標音量へフェードする(クロスフェードなど)
	inline void PublishSoundFadeEvent(EventBus& bus, SoundHandle handle, float targetVolume, float duration, bool stopOnEnd = false)
	{
		SoundFadeRequested e;
		e.handle = handle;
		e.targetVolume = targetVolume;
		e.duration = duration;
		e.stopOnEnd = stopOnEnd;
		bus.Publish(e);
	}

	// フェードアウトして停止する
	inline void PublishSoundFadeOutEvent(EventBus& bus, SoundHandle handle, float duration)
	{
		PublishSoundFadeEvent(bus, handle, 0.0f, duration, true);
	}
}