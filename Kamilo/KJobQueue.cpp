#include "KJobQueue.h"
//
#include <Windows.h>
#include <mutex>
#include <vector>
#include <unordered_set>
#include "KInternal.h"

namespace Kamilo {

static const int JOBQUEUE_IDLE_WAIT_MSEC = 5;

class CJobQueueImpl {
	struct JQITEM {
		KJOBID id;
		K_JobFunc runfunc;
		K_JobFunc delfunc;
		void *data;
	};
	static void jq_sleep(int msec) {
		Sleep(msec);
	}
	static void jq_deljob(JQITEM *job) {
		if (job) {
			if (job->delfunc) {
				job->delfunc(job->data);
			}
			delete job;
		}
	}
	static void jq_mainloop(CJobQueueImpl *q) {
		K__ASSERT(q);
		while (!q->m_should_abort) {
			// 次のジョブ
			q->m_mutex.lock();
			if (q->m_waiting_jobs.empty()) {
				K__ASSERT(q->m_job == NULL);
			} else {
				q->m_job = q->m_waiting_jobs.front();
				q->m_waiting_jobs.erase(q->m_waiting_jobs.begin());
			}
			q->m_mutex.unlock();

			// ジョブがあれば実行。なければ待機
			if (q->m_job) {
				// 実行
				q->m_job->runfunc(q->m_job->data);
				q->m_done_jobs.insert(q->m_job->id);
				jq_deljob(q->m_job);
				q->m_job = NULL;

			} else {
				// 待機
				jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
			}
		}
	}
private:
	KJOBID m_last_id; // 最後に発行したジョブ識別子
	JQITEM *m_job; // 実行中のジョブ
	std::vector<JQITEM*> m_waiting_jobs; // 待機中のジョブ
	std::unordered_set<KJOBID> m_done_jobs; // 完了のジョブ
	std::mutex m_mutex;
	std::thread m_thread; // ジョブを処理するためのスレッド
	bool m_should_abort; // 中断命令
public:
	CJobQueueImpl() {
		m_last_id = 0;
		m_job = NULL;
		m_should_abort = false;
		m_thread = std::thread(jq_mainloop, this);
	}
	~CJobQueueImpl() {
		// ジョブを空っぽにする
		clearJobs();

		// ジョブ処理スレッドを停止
		m_should_abort = true;
		if (m_thread.joinable()) {
			m_thread.join();
		}
	}
	KJOBID pushJob(K_JobFunc runfunc, K_JobFunc delfunc, void *data) {
		m_mutex.lock();
		m_last_id++;

		JQITEM *job = new JQITEM;
		job->runfunc = runfunc;
		job->delfunc = delfunc;
		job->data = data;
		job->id = m_last_id;
		m_waiting_jobs.push_back(job);
		m_mutex.unlock();
		return m_last_id; // = job.id;
	}
	bool removeJob(KJOBID job_id) {
		bool retval = false;
		m_mutex.lock();
		if (m_job && m_job->id == job_id) {
			// 実行中のジョブは削除できない
			retval = false;

		} else if (m_done_jobs.find(job_id) != m_done_jobs.end()) {
			// 完了リストから削除
			m_done_jobs.erase(job_id);
			retval = true;

		} else {
			// 削除した場合でも、もともとキューになくて削除しなかった場合でも、
			// 指定された JOBID がキューに存在しないことに変わりはないので成功とする
			retval = true;
			for (size_t i=0; i<m_waiting_jobs.size(); i++) {
				if (m_waiting_jobs[i]->id == job_id) {
					m_waiting_jobs.erase(m_waiting_jobs.begin() + i);
					break;
				}
			}
		}
		m_mutex.unlock();
		return retval;
	}
	KJobQueue::Stat getJobState(KJOBID job_id) {
		KJobQueue::Stat retval = KJobQueue::STAT_INVALID;
		m_mutex.lock();
		if (m_job && m_job->id == job_id) {
			// ジョブは実行中
			retval = KJobQueue::STAT_RUNNING;

		} else if (m_done_jobs.find(job_id) != m_done_jobs.end()) {
			// ジョブは実行済み
			retval = KJobQueue::STAT_DONE;

		} else {
			// 待機中？
			for (size_t i=0; i<m_waiting_jobs.size(); i++) {
				const JQITEM *job = m_waiting_jobs[i];
				if (job->id == job_id) {
					retval = KJobQueue::STAT_WAITING;
					break;
				}
			}
		}
		m_mutex.unlock();
		return retval;
	}
	int getRestJobCount() {
		int cnt;
		m_mutex.lock();
		cnt = (int)m_waiting_jobs.size(); // 未実行のジョブ
		if (m_job) cnt++; // 実行中のジョブ
		m_mutex.unlock();
		return cnt;
	}
	bool raiseJobPriority(KJOBID job_id) {
		// ジョブが待機列に入っているなら、待機列先頭に移動する。
		// ジョブが先頭に移動した（または初めから先頭にいた）なら true を返す
		bool retval = false;
		m_mutex.lock();

		// 目的のジョブはどこにいる？
		int job_index = -1;
		{
			for (size_t i=0; i<m_waiting_jobs.size(); i++) {
				JQITEM *job = m_waiting_jobs[i];
				if (job->id == job_id) {
					job_index = (int)i;
					break;
				}
			}
		}

		if (job_index == 0) {
			// 既に先頭にいる
			retval = true;

		} else if (job_index > 0) {
			// 待機列の先頭以外の場所にいる。
			// 先頭へ移動する
			JQITEM *job = m_waiting_jobs[job_index];
			m_waiting_jobs.erase(m_waiting_jobs.begin() + job_index);
			m_waiting_jobs.insert(m_waiting_jobs.begin(), job);
			K::print("Job(%d) moved to top of the waiting list", job_id);
			retval = true;
		}

		m_mutex.unlock();
		return retval;
	}
	void waitJob(KJOBID job_id) {
		// 待機中の場合は、非待機状態になる待つ。
		// ちなみに「KJobStart_Done になるまで待機」という方法はダメ。
		// abort 等で強制中断命令が出た場合など KJobStart_Done にならないままジョブが削除される場合がある
		while (getJobState(job_id) == KJobQueue::STAT_WAITING) {
			jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
		}

		// 処理中になっている場合は、非処理状態になるまで待つ
		while (getJobState(job_id) == KJobQueue::STAT_RUNNING) {
			jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
		}
	}
	void waitAllJobs() {
		while (getRestJobCount() > 0) {
			jq_sleep(JOBQUEUE_IDLE_WAIT_MSEC);
		}
	}
	void clearJobs() {
		// 現時点で待機列にあるジョブを削除
		m_mutex.lock();
		{
			for (size_t i=0; i<m_waiting_jobs.size(); i++) {
				jq_deljob(m_waiting_jobs[i]);
			}
			m_waiting_jobs.clear();
		}
		m_mutex.unlock();

		// 実行中のジョブの終了を待つ
		waitAllJobs();
	}
};

#pragma region KJobQueue
KJobQueue::KJobQueue() {
	CJobQueueImpl *impl = new CJobQueueImpl();
	m_Impl = std::shared_ptr<CJobQueueImpl>(impl);
}
KJOBID KJobQueue::pushJob(K_JobFunc runfunc, K_JobFunc delfunc, void *data) {
	return m_Impl->pushJob(runfunc, delfunc, data);
}
int KJobQueue::getRestJobCount() {
	return m_Impl->getRestJobCount();
}
bool KJobQueue::raiseJobPriority(KJOBID job_id) {
	return m_Impl->raiseJobPriority(job_id);
}
KJobQueue::Stat KJobQueue::getJobState(KJOBID job_id) {
	return m_Impl->getJobState(job_id);
}
void KJobQueue::waitJob(KJOBID job_id) {
	return m_Impl->waitJob(job_id);
}
void KJobQueue::waitAllJobs() {
	return m_Impl->waitAllJobs();
}
bool KJobQueue::removeJob(KJOBID job_id) {
	return m_Impl->removeJob(job_id);
}
void KJobQueue::clearJobs() {
	m_Impl->clearJobs();
}
#pragma endregion // KJobQueue

} // namespace
