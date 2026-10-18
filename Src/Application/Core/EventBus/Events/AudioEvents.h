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

	inline SoundHandle PublishSoundPlayEvent(EventBus& bus, std::string_view soundId, bool loop = false, float volume = 1.0f, float pitch = 0.0f)
	{
		SoundPlayRequested e;
		e.soundId = soundId;
		e.loop = loop;
		e.volume = volume;
		e.pitch = pitch;
		e.handle = SoundHandle::Issue();
		bus.Publish(e);

		return e.handle;
	}

	inline SoundHandle PublishSoundPlay3DEvent(EventBus& bus, std::string_view soundId, const Math::Vector3& pos, bool loop = false, float volume = 1.0f, float pitch = 0.0f)
	{
		SoundPlayRequested e;
		e.soundId = soundId;
		e.pos = pos;
		e.is3D = true;
		e.loop = loop;
		e.volume = volume;
		e.pitch = pitch;
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
}
