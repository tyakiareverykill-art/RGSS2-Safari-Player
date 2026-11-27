# Diagnostics for the mkxp-z CRuby core. Installed before game scripts.
module SafariTrace
  @last_sample = {}
  @recent = {}
  @last_frame_sample = nil
  @frames = 0

  def self.now
    Process.clock_gettime(Process::CLOCK_MONOTONIC)
  end

  def self.mark(message)
    puts "[RGSS TRACE] #{message}"
    STDOUT.flush
  rescue StandardError
    # Game calls must still run if the logging stream is unavailable.
  end

  def self.argument(value)
    value.is_a?(String) ? value[0, 140].inspect : value.class.to_s
  end

  def self.begin_asset(label, value)
    started = now
    @recent[label] = argument(value)
    return if started - @last_sample.fetch(label, -10) < 1.0

    @last_sample[label] = started
    mark("#{label} begin #{argument(value)}")
    started
  end

  def self.end_asset(label, started)
    mark("#{label} end #{((now - started) * 1000).round}ms") if started
  end

  def self.after_frame
    @frames += 1
    current = now
    return if @last_frame_sample && current - @last_frame_sample < 2.0

    map = defined?($game_map) && $game_map
    map_id = map ? map.instance_variable_get(:@map_id) : nil
    scene = defined?($scene) && $scene
    assets = @recent.map { |kind, value| "#{kind}=#{value}" }.join(" ")
    rate = @last_frame_sample ? (@frames / (current - @last_frame_sample)).round(1) : "-"
    mark("Graphics.update count=#{@frames} fps=#{rate} map=#{map_id || '-'} scene=#{scene ? scene.class : '-'} #{assets}")
    @frames = 0
    @last_frame_sample = current
  end

  def self.wrap_asset(owner, method, label)
    return unless owner.method_defined?(method) || owner.private_method_defined?(method)

    wrapper = Module.new do
      define_method(method) do |*args, &block|
        token = SafariTrace.begin_asset(label, args.first)
        result = super(*args, &block)
        SafariTrace.end_asset(label, token)
        result
      rescue StandardError => error
        SafariTrace.mark("#{label} exception #{error.class}")
        raise
      end
      private method if owner.private_method_defined?(method)
    end
    owner.prepend(wrapper)
  end
end

SafariTrace.mark("preload active")
SafariTrace.wrap_asset(Bitmap, :initialize, "Bitmap") if defined?(Bitmap)
if defined?(Audio)
  [:bgm_play, :bgs_play, :me_play, :se_play].each do |method|
    SafariTrace.wrap_asset(Audio.singleton_class, method, "Audio.#{method}")
  end
end
if defined?(Graphics) && Graphics.respond_to?(:update)
  Graphics.singleton_class.prepend(Module.new do
    def update(*args, &block)
      result = super(*args, &block)
      SafariTrace.after_frame
      result
    end
  end)
end
