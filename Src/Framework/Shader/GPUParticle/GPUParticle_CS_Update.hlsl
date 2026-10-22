#include "inc_GPUParticleShader.hlsli"

// パーティクル本体バッファ(読み書き)
RWStructuredBuffer<Particle> g_ParticleBuffer : register(u0);

cbuffer cbGPUParticleUpdate : register(b0)
{
	uint	g_MaxParticleNum;	// パーティクルの最大数
	float	g_DeltaTime;		// 前フレームからの経過時間
	float2	_pad0;

	float3	g_Gravity;			// 重力(加速度)
	float	_pad1;

	// ※必ず末尾に追加する事(C++側cbUpdateと一致させる)
	float3	g_FollowDelta;		// 生きている全粒子の平行移動量(発生源への追従用)
	float	_pad2;
};

static const int EMIT_DIST_CONVERGE = 2;

//================================
// 生存中のパーティクルを1フレームぶんシミュレーションする
// ※スレッド数はC++側でMaxParticleNumぶん(256の倍数に切り上げ)Dispatchする
//================================
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint index = id.x;

	if (index >= g_MaxParticleNum) { return; }

	Particle p = g_ParticleBuffer[index];

	// 死亡中のパーティクルは何もしない
	if (p.Life <= 0) { return; }

	// 発生源への追従：粒子と収束先を同じ量だけ平行移動する(相対位置は保たれる)
	p.Position += g_FollowDelta;
	p.ConvergeCenter += g_FollowDelta;

	// Beam(固定軸方向に伸びる線)は物理挙動の対象外：重力はもちろん、速度による移動も一切適用しない。
	// Beamは「静止したまま軸方向に伸縮するマーカー」という定義そのものなので、
	// 万が一Beamのレイヤーに初速を設定してしまっても動かないようにここで保証しておく
	// モード値を整数化
	int distribution = (int) (p.Distribution + 0.5f);

	// 収束時刻に達して中心へ確定した粒子(1フレームだけ表示を残す)
	bool snapped = false;

	if (p.BillboardMode < 1.5f)
	{
    // --- 挙動の更新 ---
    
		if (distribution == EMIT_DIST_CONVERGE)
		{
        // 【Convergeモード特有の更新処理】
        // 例: 中心に向かって加速させる、または中心付近で消滅させる 等
			float3 toCenter = p.ConvergeCenter - p.Position;
			float dist = length(toCenter);

			// 到達判定：1フレームで届く距離、または今フレームで寿命が尽きる(収束時刻に達する)時
			bool arriving = (dist <= length(p.Velocity) * g_DeltaTime) || (p.Life <= g_DeltaTime);
			if (arriving)
			{
				if (dist <= 1e-3f)
				{
					p.Life = 0.0f; // 中心で1フレーム表示済み：消滅
				}
				else
				{
					p.Position = p.ConvergeCenter; // 中心へ確定(通り抜け防止)
					p.Velocity = float3(0.0f, 0.0f, 0.0f);
					snapped = true;
				}
			}
		}
		else
		{
        // 通常の物理運動（重力適用）
			p.Velocity += g_Gravity * g_DeltaTime;
		}

    // 位置の更新
		p.Position += p.Velocity * g_DeltaTime;
	}

	// 寿命を減らす
	p.Life -= g_DeltaTime;
	if (p.Life < 0) { p.Life = 0; }

	// 中心へ確定した粒子は、次のUpdateで消えるまで1フレームだけ生かす
	if (snapped) { p.Life = 0.0001f; }

	g_ParticleBuffer[index] = p;
}
