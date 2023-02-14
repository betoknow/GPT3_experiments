import gym
import numpy as np
from flask import Flask, render_template

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/train')
def train():
    env = gym.make('InvertedPendulum-v2')
    state_size = env.observation_space.shape[0]
    action_size = env.action_space.shape[0]
    agent = Agent(state_size, action_size)
    agent.train(env)
    return "Training Complete!"

class Agent:
    def __init__(self, state_size, action_size):
        self.state_size = state_size
        self.action_size = action_size
        self.gamma = 0.99
        self.learning_rate = 0.001
        self.states = []
        self.gradients = []
        self.rewards = []
        self.probs = []
        self.model = self._build_model()
        
    def _build_model(self):
        # Neural Net for Deep-Q learning Model
        model = Sequential()
        model.add(Dense(24, input_dim=self.state_size, activation='relu'))
        model.add(Dense(24, activation='relu'))
        model.add(Dense(self.action_size, activation='linear'))
        model.compile(loss='mse',
                      optimizer=Adam(lr=self.learning_rate))
        return model
    
    def remember(self, state, action, prob, reward):
        y = np.zeros([self.action_size])
        y[action] = 1
        self.gradients.append(np.array(y).astype('float32') - prob)
        self.states.append(state)
        self.rewards.append(reward)
    
    def act(self, state):
        state = state.reshape([1, state.shape[0]])
        aprob = self.model.predict(state, batch_size=1).flatten()
        self.probs.append(aprob)
        prob = aprob / np.sum(aprob)
        action = np.random.choice(self.action_size, 1, p=prob)[0]
        return action, prob
    
    def discount_rewards(self, rewards):
        discounted_rewards = np.zeros_like(rewards)
        running_add = 0
        for t in reversed(range(0, rewards.size)):
            if rewards[t] != 0:
                running_add = 0
            running_add = running_add * self.gamma + rewards[t]
            discounted_rewards[t] = running_add
        return discounted_rewards
    
    def train(self, env):
        for e in range(episodes):
            state = env.reset()
            total_reward = 0
            done = False
            while not done:
                action, prob = self.act(state)
                state_next, reward, done, info = env.step(action)
                self.remember(state, action, prob, reward)
                state = state_next
                total_reward += reward
            self.rewards.append(total_reward)
            discounts = self.discount_rewards(np.vstack(self.rewards))
            discounts -= np.mean(discounts)
            discounts /= np.std(discounts)
            gradients = np.vstack(self.gradients)
            gradients *= discounts
            X = np.squeeze(np.vstack([self.states]))
            Y = self.probs + self.learning_rate * np.squeeze(np.vstack([gradients]))
            self.model.train_on_batch(X, Y)
            self.states, self.probs, self.gradients, self.rewards = [], [], [], []

if __name__ == '__main__':
    app.run()